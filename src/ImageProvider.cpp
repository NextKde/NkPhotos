// SPDX-License-Identifier: GPL-3.0-only
#include "ImageProvider.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QColorSpace>
#include <QImageIOHandler>
#include <QProcess>
#include <QStandardPaths>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QScopeGuard>

namespace {
QImage videoPoster(const ImageSource &source)
{
    const QFileInfo before(source.path);
    if (!before.isFile() || before.isSymLink() || !before.isReadable()
        || before.size() != source.bytes || before.lastModified().toMSecsSinceEpoch() != source.modified) return {};
    static QSemaphore posterSlots(2);
    if (!posterSlots.tryAcquire(1, 100)) return {};
    const auto release = qScopeGuard([&] { posterSlots.release(); });
    const QString executable = QStandardPaths::findExecutable("ffmpeg");
    if (executable.isEmpty()) return {};
    QProcess process;
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(executable, {"-nostdin", "-v", "error", "-threads", "1",
        "-protocol_whitelist", "file,pipe", "-i", source.path,
        "-map", "0:v:0", "-frames:v", "1", "-an", "-sn", "-dn",
        "-vf", "scale=384:384:force_original_aspect_ratio=decrease",
        "-threads", "1", "-f", "image2pipe", "-c:v", "png", "pipe:1"});
    if (!process.waitForStarted(1000)) return {};
    QElapsedTimer timer;
    timer.start();
    QByteArray bytes;
    while (process.state() != QProcess::NotRunning && timer.elapsed() < 5000) {
        process.waitForReadyRead(50);
        bytes += process.readAllStandardOutput();
        if (bytes.size() > 2 * 1024 * 1024) break;
    }
    if (process.state() != QProcess::NotRunning) {
        process.kill(); process.waitForFinished(1000); return {};
    }
    bytes += process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
        || bytes.size() > 2 * 1024 * 1024) return {};
    QFileInfo after(source.path);
    if (after.size() != source.bytes || after.lastModified().toMSecsSinceEpoch() != source.modified) return {};
    return QImage::fromData(bytes, "PNG");
}
}

ImageProvider::ImageProvider(std::shared_ptr<ImageRegistry> registry)
    : QQuickImageProvider(Image, ForceAsynchronousImageLoading), m_registry(std::move(registry)) {}

QString ImageProvider::token(const ImageSource &source)
{
    QByteArray key = source.path.toUtf8();
    key += '\0'; key += QByteArray::number(source.bytes);
    key += '\0'; key += QByteArray::number(source.modified);
    return QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex());
}

QImage ImageProvider::decode(const ImageSource &source, QSize requestedSize, QSize *original)
{
    QFileInfo info(source.path);
    if (!info.isFile() || info.isSymLink() || info.size() != source.bytes
        || info.lastModified().toMSecsSinceEpoch() != source.modified
        || info.size() > 64 * 1024 * 1024 || info.size() <= 0) return {};
    QImageReader reader(source.path);
    reader.setAutoTransform(true);
    const QSize native = reader.size();
    const qint64 pixels = qint64(native.width()) * native.height();
    if (!native.isValid() || pixels > 120000000 || native.width() > 40000 || native.height() > 40000) return {};
    // Decoders unable to scale during reading must fit the full-frame budget.
    if (!reader.supportsOption(QImageIOHandler::ScaledSize) && pixels > 32000000) return {};
    if (original) *original = native;
    requestedSize = QSize(qBound(1, requestedSize.width(), 4096), qBound(1, requestedSize.height(), 4096));
    reader.setScaledSize(native.scaled(requestedSize, Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull()) return {};
    // Plugins may ignore scaledSize. Never retain an unbounded frame.
    if (image.width() > requestedSize.width() || image.height() > requestedSize.height())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.colorSpace().isValid() && image.colorSpace() != QColorSpace(QColorSpace::SRgb))
        image.convertToColorSpace(QColorSpace::SRgb);
    QFileInfo after(source.path);
    if (after.size() != source.bytes || after.lastModified().toMSecsSinceEpoch() != source.modified) return {};
    return image;
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString key = id.section('/', 0, 0);
    ImageSource source;
    {
        QReadLocker locker(&m_registry->lock);
        const auto it = m_registry->sources.constFind(key);
        if (it == m_registry->sources.cend()) return {};
        source = it.value();
    }
    const bool thumbnail = id.endsWith("/thumb");
    const QSize target = thumbnail ? QSize(384, 384) : (requestedSize.isValid() ? requestedSize : QSize(4096, 4096));
    if (thumbnail) {
        QMutexLocker locker(&m_cacheMutex);
        if (const auto cached = m_cache.object(key)) {
            if (size) *size = cached->size();
            return *cached;
        }
    }
    const QString extension = QFileInfo(source.path).suffix().toLower();
    static const QStringList videoExtensions{"mp4", "m4v", "mov", "mkv", "webm", "avi", "ogv", "mpeg", "mpg", "3gp", "mts", "m2ts"};
    QImage image = videoExtensions.contains(extension) ? videoPoster(source) : decode(source, target);
    if (size) *size = image.size();
    if (thumbnail && !image.isNull()) {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(key, new QImage(image), int((image.sizeInBytes() + 1023) / 1024));
    }
    return image;
}
