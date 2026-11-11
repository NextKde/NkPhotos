// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LibraryStore.h"
#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>
#include <atomic>
#include <deque>
#include <functional>

class PhotoLibrary : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QUrl folder READ folder NOTIFY changed)
    Q_PROPERTY(QString collectionId READ collectionId NOTIFY changed)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY changed)
    Q_PROPERTY(bool newestFirst READ newestFirst WRITE setNewestFirst NOTIFY changed)
    Q_PROPERTY(QString dateFilter READ dateFilter WRITE setDateFilter NOTIFY changed)
    Q_PROPERTY(QString mediaFilter READ mediaFilter WRITE setMediaFilter NOTIFY changed)
    Q_PROPERTY(QString dateView READ dateView WRITE setDateView NOTIFY changed)
    Q_PROPERTY(QVariantList dateGroups READ dateGroups NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList albums READ albums NOTIFY collectionsChanged)
    Q_PROPERTY(QVariantList memories READ memories NOTIFY collectionsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY operationChanged)
    Q_PROPERTY(double progress READ progress NOTIFY operationChanged)
    Q_PROPERTY(QString operationText READ operationText NOTIFY operationChanged)
    Q_PROPERTY(QVariantList operationResults READ operationResults NOTIFY operationChanged)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList fileFilters READ fileFilters CONSTANT)
public:
    enum Roles { NameRole = Qt::UserRole + 1, UrlRole, ImageRole, FavoriteRole, ModifiedRole,
                 IdRole, MediaTypeRole, DateRole, DateSourceRole, TitleRole, HiddenRole,
                 TrashedRole, AvailableRole, ManagedRole, WidthRole, HeightRole,
                 DurationRole, BatchSelectedRole, DescriptionRole, KeywordsRole };
    explicit PhotoLibrary(std::shared_ptr<ImageRegistry> registry, QObject *parent = nullptr);
    ~PhotoLibrary() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariantList folders() const { return m_snapshot.folders; }
    QString mode() const { return m_mode; }
    QUrl folder() const { return m_folder; }
    QString collectionId() const { return m_collectionId; }
    QString search() const { return m_search; }
    bool newestFirst() const { return m_newestFirst; }
    QString dateFilter() const { return m_dateFilter; }
    QString mediaFilter() const { return m_mediaFilter; }
    QString dateView() const { return m_dateView; }
    QVariantList dateGroups() const;
    int selectedIndex() const;
    QVariantMap selected() const;
    QStringList selectedIds() const;
    int selectionCount() const { return m_selection.size(); }
    QVariantList albums() const { return m_snapshot.albums; }
    QVariantList memories() const { return m_snapshot.memories; }
    bool loading() const { return m_loading; }
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString operationText() const { return m_operationText; }
    QVariantList operationResults() const { return m_operationResults; }
    QString message() const { return m_message; }
    QStringList fileFilters() const;
    void setSearch(const QString &value);
    void setNewestFirst(bool value);
    void setDateFilter(const QString &value);
    void setMediaFilter(const QString &value);
    void setDateView(const QString &value);
    Q_INVOKABLE void addFolder(const QUrl &url);
    Q_INVOKABLE void removeFolder(const QUrl &url);
    Q_INVOKABLE void relinkFolder(const QUrl &oldUrl, const QUrl &newUrl);
    Q_INVOKABLE void showCollection(const QString &mode, const QUrl &folder = {});
    Q_INVOKABLE void showAlbum(const QString &id);
    Q_INVOKABLE void showMemory(const QString &id);
    Q_INVOKABLE void openFiles(const QList<QUrl> &urls);
    // Viewing focus is independent of the stable-ID batch selection.
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void selectById(const QString &id);
    Q_INVOKABLE void selectForBatch(int index, int modifiers = 0);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QStringList actionIds() const;
    Q_INVOKABLE QVariantMap entry(const QString &id) const;
    Q_INVOKABLE void navigate(int delta);
    Q_INVOKABLE void toggleFavorite();
    // Every destructive/async operation takes the IDs captured by its caller.
    Q_INVOKABLE void setFavorite(const QStringList &ids, bool value);
    Q_INVOKABLE void setHidden(const QStringList &ids, bool value);
    Q_INVOKABLE void moveToTrash(const QStringList &ids);
    Q_INVOKABLE void restore(const QStringList &ids);
    Q_INVOKABLE void permanentlyDelete(const QStringList &ids);
    Q_INVOKABLE void updateMetadata(const QString &id, const QString &title,
                                   const QString &description, const QStringList &keywords);
    Q_INVOKABLE void importFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void exportOriginals(const QStringList &ids, const QUrl &destination);
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE void createAlbum(const QString &name, const QStringList &ids = {});
    Q_INVOKABLE void renameAlbum(const QString &id, const QString &name);
    Q_INVOKABLE void deleteAlbum(const QString &id);
    Q_INVOKABLE void addToAlbum(const QString &albumId, const QStringList &ids);
    Q_INVOKABLE void removeFromAlbum(const QString &albumId, const QStringList &ids);
    Q_INVOKABLE void setAlbumCover(const QString &albumId, const QString &mediaId);
    Q_INVOKABLE void createMemory(const QString &name, const QStringList &ids);
    Q_INVOKABLE void createDateMemory(const QString &date, const QString &name);
    Q_INVOKABLE void updateMemory(const QString &id, const QString &name,
                                 const QStringList &ids, const QString &coverId,
                                 double seconds, const QUrl &music, bool muted);
    Q_INVOKABLE void deleteMemory(const QString &id);
    Q_INVOKABLE QVariantList memoryItems(const QString &id) const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void dismissMessage();
    static ScanResult scan(const QStringList &directories, const QList<QUrl> &files,
                           const QStringList &extensions, const std::shared_ptr<std::atomic_bool> &cancel);
signals:
    void changed();
    void foldersChanged();
    void collectionsChanged();
    void selectionChanged();
    void operationChanged();
    void viewRequested();
    void galleryRequested();
    void albumCreated(const QString &id);
    void memoryCreated(const QString &id);
private:
    using Work = std::function<LibrarySnapshot(LibraryStore &)>;
    struct Job { Work work; QString label; bool cancellable = false, scan = false; };
    void enqueue(Work work, const QString &label = {}, bool cancellable = false, bool scan = false);
    bool requireLibraryIds(const QStringList &ids);
    void runOperation(std::function<LibrarySnapshot(LibraryStore &, const std::shared_ptr<std::atomic_bool> &, const LibraryStore::Progress &)> work,
                      const QString &label);
    void startNext();
    void applySnapshot(LibrarySnapshot result);
    void rebuild();
    void scheduleScan();
    void startScan();
    void updateWatches();
    void persist();
    void changeCollection(const QString &mode, const QUrl &folder, const QString &id);
    void selectionUpdated();
    QVariantMap entryMap(const PhotoEntry &item) const;
    std::shared_ptr<ImageRegistry> m_registry;
    QSettings m_settings;
    QString m_root;
    QStringList m_extensions;
    LibrarySnapshot m_snapshot;
    QList<PhotoEntry> m_visible, m_sessionEntries;
    QList<QUrl> m_session;
    QString m_mode = "all", m_collectionId, m_search, m_message;
    QString m_dateFilter, m_mediaFilter = "all", m_dateView = "all";
    QUrl m_folder;
    QString m_selected, m_anchor;
    QSet<QString> m_selection;
    QHash<QString, QString> m_focusByCollection;
    QUrl m_requestedSelection;
    bool m_newestFirst = true, m_loading = false, m_busy = false;
    bool m_openAfterScan = false, m_scanQueued = false, m_scanAgain = false, m_closing = false;
    double m_progress = 0;
    QString m_operationText;
    QVariantList m_operationResults;
    quint64 m_sessionGeneration = 0;
    QThreadPool m_pool;
    QFutureWatcher<LibrarySnapshot> m_worker;
    std::deque<Job> m_jobs;
    std::shared_ptr<std::atomic_bool> m_scanCancel, m_operationCancel;
    QFileSystemWatcher m_files;
    QTimer m_debounce;
};
