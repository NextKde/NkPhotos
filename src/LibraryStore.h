// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ImageProvider.h"
#include <QDateTime>
#include <QSet>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariant>
#include <atomic>
#include <functional>

struct PhotoEntry {
    QUrl url;
    QString name;
    ImageSource source;
    QString token;
    QString id, mediaType = "photo", title, description, dateSource = "file";
    QStringList keywords;
    QString camera;
    qint64 date = 0, importedAt = 0, deletedAt = 0, duration = 0;
    int width = 0, height = 0;
    bool favorite = false, hidden = false, managed = false, available = true, tombstone = false;
    QStringList sourceIds, albumIds;
    bool standalone = false;
    bool operator==(const PhotoEntry &) const = default;
};
struct ScanResult {
    QList<PhotoEntry> entries;
    QStringList unavailable;
    bool limited = false;
};
struct LibrarySnapshot {
    QList<PhotoEntry> entries;
    QVariantList folders, albums, memories, results;
    QString error, createdAlbum, createdMemory;
    QList<PhotoEntry> sessionEntries;
    quint64 sessionGeneration = 0;
    bool sessionResult = false, scanComplete = false;
    bool ready = false;
};

// Short-lived, worker-thread-only SQLite connection. No connection crosses a thread.
class LibraryStore {
public:
    using Progress = std::function<void(double, const QString &)>;
    explicit LibraryStore(const QString &root);
    ~LibraryStore();
    bool open(const QStringList &legacyFolders, const QStringList &legacyFavorites);
    LibrarySnapshot snapshot();
    LibrarySnapshot scanSources(const QStringList &extensions,
                                const std::shared_ptr<std::atomic_bool> &cancel);
    LibrarySnapshot addFolder(const QUrl &url);
    LibrarySnapshot removeFolder(const QUrl &url);
    LibrarySnapshot relinkFolder(const QUrl &oldUrl, const QUrl &newUrl);
    LibrarySnapshot setFlag(const QStringList &ids, const QString &field, bool value);
    LibrarySnapshot metadata(const QString &id, const QString &title,
                             const QString &description, const QStringList &keywords);
    LibrarySnapshot trash(const QStringList &ids, const QString &action,
                          const std::shared_ptr<std::atomic_bool> &cancel, const Progress &progress);
    LibrarySnapshot copyFiles(const QList<QUrl> &urls, const QStringList &ids,
                              const QUrl &destination, bool importing,
                              const std::shared_ptr<std::atomic_bool> &cancel, const Progress &progress);
    LibrarySnapshot album(const QString &action, const QString &id, const QString &name,
                          const QStringList &ids);
    LibrarySnapshot memory(const QString &action, const QString &id, const QString &name,
                           const QStringList &ids, const QString &cover, double seconds,
                           const QUrl &music, bool muted);
    void recover();
    static PhotoEntry inspect(const QString &path);
    static ScanResult scan(const QStringList &directories, const QList<QUrl> &files,
                           const QStringList &extensions, const std::shared_ptr<std::atomic_bool> &cancel);
private:
    bool execute(const QString &sql, const QVariantList &values = {});
    bool begin();
    bool commit();
    void rollback();
    bool saveEntry(const PhotoEntry &entry);
    void generateDateMemories(const std::shared_ptr<std::atomic_bool> &cancel);
    PhotoEntry find(const QString &id);
    QString idForPath(const QString &path);
    bool ownedPath(const QString &path) const;
    bool finalizeOperation(const QString &operation,
                           const std::shared_ptr<std::atomic_bool> &cancel = {});
    void recordResult(const QString &id, const QUrl &url, const QString &status, const QString &error = {});
    QString journal(const QString &type, const QString &id, const QString &src,
                    const QString &dst, const QString &temporary, const QVariantMap &payload = {});
    void failOperation(const QString &operation, const QString &error, const QString &state = "failed");
    QString m_root, m_connection, m_error;
    QSqlDatabase m_db;
    QVariantList m_results;
    bool m_ready = false;
};
