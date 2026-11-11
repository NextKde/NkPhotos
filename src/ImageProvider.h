// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickImageProvider>
#include <QReadWriteLock>
#include <QCache>
#include <QMutex>
#include <memory>

struct ImageSource {
    QString path;
    qint64 bytes = 0;
    qint64 modified = 0;
};
struct ImageRegistry {
    QReadWriteLock lock;
    QHash<QString, ImageSource> sources;
};

class ImageProvider : public QQuickImageProvider {
public:
    explicit ImageProvider(std::shared_ptr<ImageRegistry> registry);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    static QImage decode(const ImageSource &source, QSize requestedSize, QSize *original = nullptr);
    static QString token(const ImageSource &source);
private:
    std::shared_ptr<ImageRegistry> m_registry;
    QMutex m_cacheMutex;
    QCache<QString, QImage> m_cache{48 * 1024}; // KiB; thumbnails only
};
