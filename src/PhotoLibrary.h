// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ImageProvider.h"
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <atomic>

struct PhotoEntry {
    QUrl url;
    QString name;
    ImageSource source;
    QString token;
};
struct ScanResult {
    QList<PhotoEntry> entries;
    QStringList unavailable;
    bool limited = false;
};

class PhotoLibrary : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QUrl folder READ folder NOTIFY changed)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY changed)
    Q_PROPERTY(bool newestFirst READ newestFirst WRITE setNewestFirst NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList fileFilters READ fileFilters CONSTANT)
public:
    enum Roles { NameRole = Qt::UserRole + 1, UrlRole, ImageRole, FavoriteRole, ModifiedRole };
    explicit PhotoLibrary(std::shared_ptr<ImageRegistry> registry, QObject *parent = nullptr);
    ~PhotoLibrary() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariantList folders() const;
    QString mode() const { return m_mode; }
    QUrl folder() const { return m_folder; }
    QString search() const { return m_search; }
    bool newestFirst() const { return m_newestFirst; }
    int selectedIndex() const;
    QVariantMap selected() const;
    bool loading() const { return m_loading; }
    QString message() const { return m_message; }
    QStringList fileFilters() const;
    void setSearch(const QString &value);
    void setNewestFirst(bool value);
    Q_INVOKABLE void addFolder(const QUrl &url);
    Q_INVOKABLE void removeFolder(const QUrl &url);
    Q_INVOKABLE void showCollection(const QString &mode, const QUrl &folder = {});
    Q_INVOKABLE void openFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void navigate(int delta);
    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void dismissMessage();
    static ScanResult scan(const QStringList &directories, const QList<QUrl> &files,
                           const QStringList &extensions, const std::shared_ptr<std::atomic_bool> &cancel);
signals:
    void changed();
    void viewRequested();
private:
    void rebuild();
    void scheduleScan();
    void startScan();
    void updateWatches();
    void persist();
    std::shared_ptr<ImageRegistry> m_registry;
    QSettings m_settings;
    QStringList m_folders, m_favorites, m_extensions;
    QList<PhotoEntry> m_entries, m_visible;
    QList<QUrl> m_session;
    QString m_mode = "all", m_search, m_message;
    QUrl m_folder, m_selected, m_requestedSelection;
    bool m_newestFirst = true, m_loading = false, m_pending = false, m_openAfterScan = false;
    quint64 m_generation = 0, m_runningGeneration = 0;
    QThreadPool m_pool;
    QFutureWatcher<ScanResult> m_scan;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QFileSystemWatcher m_files;
    QTimer m_debounce;
};
