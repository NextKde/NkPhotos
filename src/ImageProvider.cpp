// SPDX-License-Identifier: GPL-3.0-only
#include "ImageProvider.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QColorSpace>
#include <QImageIOHandler>

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
    QImage image = decode(source, target);
    if (size) *size = image.size();
    if (thumbnail && !image.isNull()) {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(key, new QImage(image), int((image.sizeInBytes() + 1023) / 1024));
    }
    return image;
}
