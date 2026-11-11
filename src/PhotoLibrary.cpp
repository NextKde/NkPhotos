// SPDX-License-Identifier: GPL-3.0-only
#include "PhotoLibrary.h"
#include <QtConcurrentRun>
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QPointer>
#include <QElapsedTimer>
#include <algorithm>

namespace {
QString collectionKey(const QString &mode, const QUrl &folder, const QString &id)
{
    return mode + '\n' + folder.toString() + '\n' + id;
}
QString dateKey(qint64 timestamp, const QString &view)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp).date().toString(view == "years" ? "yyyy" : view == "months" ? "yyyy-MM" : "yyyy-MM-dd");
}
}

PhotoLibrary::PhotoLibrary(std::shared_ptr<ImageRegistry> registry, QObject *parent)
    : QAbstractListModel(parent), m_registry(std::move(registry)),
      m_root(QFileInfo(m_settings.fileName()).absolutePath() + "/library")
{
    m_pool.setMaxThreadCount(1);
    m_newestFirst = m_settings.value("newestFirst",true).toBool();
    QMimeDatabase db;
    for (const auto &mime : QImageReader::supportedMimeTypes())
        m_extensions.append(db.mimeTypeForName(QString::fromLatin1(mime)).suffixes());
    m_extensions.append({"mp4","m4v","mov","mkv","webm","avi","mpeg","mpg","mts","m2ts","ogv","3gp"});
    m_extensions.removeDuplicates();
    m_scanCancel = std::make_shared<std::atomic_bool>(false);
    m_operationCancel = std::make_shared<std::atomic_bool>(false);
    m_debounce.setInterval(350); m_debounce.setSingleShot(true);
    connect(&m_files,&QFileSystemWatcher::directoryChanged,&m_debounce,qOverload<>(&QTimer::start));
    connect(&m_files,&QFileSystemWatcher::fileChanged,&m_debounce,qOverload<>(&QTimer::start));
    connect(&m_debounce,&QTimer::timeout,this,&PhotoLibrary::refresh);
    connect(&m_worker,&QFutureWatcher<LibrarySnapshot>::finished,this,[this] {
        if (m_closing) return;
        const auto result = m_worker.result();
        applySnapshot(result);
        m_loading = false; m_busy = false; m_progress = 1;
        if (result.scanComplete) {
            m_scanQueued = false;
            if (m_scanAgain) { m_scanAgain = false; scheduleScan(); }
        }
        emit changed(); emit operationChanged();
        startNext();
    });
    enqueue([](LibraryStore &store) { store.recover(); return store.snapshot(); });
    QTimer::singleShot(0,this,&PhotoLibrary::refresh);
}
PhotoLibrary::~PhotoLibrary()
{
    m_closing = true;
    m_scanCancel->store(true); m_operationCancel->store(true);
    m_pool.waitForDone();
}
void PhotoLibrary::enqueue(Work work, const QString &label, bool cancellable, bool scan)
{
    m_jobs.push_back({std::move(work),label,cancellable,scan});
    startNext();
}
void PhotoLibrary::startNext()
{
    if (m_loading || m_jobs.empty() || m_closing) return;
    Job job = std::move(m_jobs.front()); m_jobs.pop_front();
    m_loading = true; m_busy = !job.label.isEmpty(); m_operationText = job.label;
    if (m_busy) { m_progress = 0; m_operationResults.clear(); }
    // Each accepted operation captures its own cancellation token. Navigation never cancels it.
    if (job.cancellable) m_operationCancel->store(false);
    const QString root = m_root;
    const QStringList folders = m_settings.value("folders").toStringList();
    const QStringList favorites = m_settings.value("favorites").toStringList();
    m_worker.setFuture(QtConcurrent::run(&m_pool,[root,folders,favorites,job=std::move(job)]() mutable {
        LibraryStore store(root);
        LibrarySnapshot result;
        if (!store.open(folders,favorites)) result = store.snapshot();
        else result = job.work(store);
        result.scanComplete = job.scan;
        return result;
    }));
    emit changed(); emit operationChanged();
}
void PhotoLibrary::applySnapshot(LibrarySnapshot result)
{
    if (!result.error.isEmpty()) m_message = result.error;
    if (!result.results.isEmpty()) m_operationResults = result.results;
    if (result.ready) {
        m_snapshot = result;
        if (result.sessionResult && result.sessionGeneration == m_sessionGeneration) m_sessionEntries = result.sessionEntries;
        // Persisted files opened temporarily still reflect current flags, metadata and exclusions.
        for(auto &session:m_sessionEntries)if(!session.id.startsWith("session:")) {
            bool found=false;
            for(const auto &indexed:m_snapshot.entries)if(indexed.id==session.id){session=indexed;found=true;break;}
            if(!found){session.tombstone=true;session.available=false;}
        }
        {
            QWriteLocker locker(&m_registry->lock);
            // Registry includes every indexed item, not only current visible rows.
            m_registry->sources.clear();
            for (const auto &entry : m_snapshot.entries)
                if (entry.available) m_registry->sources.insert(entry.token,entry.source);
            for (const auto &entry : m_sessionEntries)
                if (entry.available) m_registry->sources.insert(entry.token,entry.source);
        }
        rebuild(); updateWatches();
        emit foldersChanged(); emit collectionsChanged();
        if (!result.createdAlbum.isEmpty()) emit albumCreated(result.createdAlbum);
        if (!result.createdMemory.isEmpty()) emit memoryCreated(result.createdMemory);
        if (result.sessionResult && result.sessionGeneration == m_sessionGeneration && m_mode == "session" && m_openAfterScan) {
            for (const auto &entry : m_visible) if (entry.url == m_requestedSelection) { m_selected = entry.id; break; }
            if (selectedIndex() < 0 && !m_visible.isEmpty()) m_selected = m_visible.first().id;
            m_requestedSelection.clear(); m_openAfterScan = false;
            emit changed();
            if (selectedIndex() >= 0) emit viewRequested();
        }
    }
}
int PhotoLibrary::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_visible.size()); }
QHash<int,QByteArray> PhotoLibrary::roleNames() const
{
    return {{NameRole,"photoName"},{UrlRole,"photoUrl"},{ImageRole,"imageSource"},{FavoriteRole,"favorite"},
            {ModifiedRole,"modified"},{IdRole,"mediaId"},{MediaTypeRole,"mediaType"},{DateRole,"date"},
            {DateSourceRole,"dateSource"},{TitleRole,"title"},{HiddenRole,"hidden"},{TrashedRole,"trashed"},
            {AvailableRole,"available"},{ManagedRole,"managed"},{WidthRole,"width"},{HeightRole,"height"},
            {DurationRole,"duration"},{BatchSelectedRole,"batchSelected"},{DescriptionRole,"description"},{KeywordsRole,"keywords"}};
}
QVariant PhotoLibrary::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row()<0 || index.row()>=m_visible.size()) return {};
    const auto &item = m_visible.at(index.row());
    switch (role) {
    case NameRole: return item.name;
    case UrlRole: return item.url;
    case ImageRole: return item.available ? "image://photos/"+item.token : QString();
    case FavoriteRole: return item.favorite;
    case ModifiedRole: return QDateTime::fromMSecsSinceEpoch(item.source.modified);
    case IdRole: return item.id;
    case MediaTypeRole: return item.mediaType;
    case DateRole: return QDateTime::fromMSecsSinceEpoch(item.date);
    case DateSourceRole: return item.dateSource;
    case TitleRole: return item.title;
    case HiddenRole: return item.hidden;
    case TrashedRole: return item.deletedAt>0;
    case AvailableRole: return item.available;
    case ManagedRole: return item.managed;
    case WidthRole: return item.width;
    case HeightRole: return item.height;
    case DurationRole: return item.duration;
    case BatchSelectedRole: return m_selection.contains(item.id);
    case DescriptionRole: return item.description;
    case KeywordsRole: return item.keywords;
    default: return {};
    }
}
QVariantMap PhotoLibrary::entryMap(const PhotoEntry &item) const
{
    return {{"id",item.id},{"mediaId",item.id},{"name",item.name},{"url",item.url},{"path",item.source.path},
            {"imageSource",item.available ? "image://photos/"+item.token : QString()},
            {"bytes",item.source.bytes},{"modified",QDateTime::fromMSecsSinceEpoch(item.source.modified)},
            {"date",QDateTime::fromMSecsSinceEpoch(item.date)},{"dateSource",item.dateSource},
            {"mediaType",item.mediaType},{"title",item.title},{"description",item.description},{"keywords",item.keywords},
            {"favorite",item.favorite},{"hidden",item.hidden},{"trashed",item.deletedAt>0},{"managed",item.managed},
            {"available",item.available},{"width",item.width},{"height",item.height},{"duration",item.duration},
            {"camera",item.camera},{"deletedAt",item.deletedAt ? QVariant(QDateTime::fromMSecsSinceEpoch(item.deletedAt)) : QVariant()},
            {"importedAt",QDateTime::fromMSecsSinceEpoch(item.importedAt)},{"batchSelected",m_selection.contains(item.id)},
            {"temporary",item.id.startsWith("session:")}};
}
QVariantMap PhotoLibrary::entry(const QString &id) const
{
    for (const auto &item : m_snapshot.entries) if (item.id == id) return entryMap(item);
    for (const auto &item : m_sessionEntries) if (item.id == id) return entryMap(item);
    return {};
}
int PhotoLibrary::selectedIndex() const
{
    for (int i=0;i<m_visible.size();++i) if (m_visible.at(i).id==m_selected) return i;
    return -1;
}
QVariantMap PhotoLibrary::selected() const
{
    const int i=selectedIndex(); return i<0 ? QVariantMap() : entryMap(m_visible.at(i));
}
QStringList PhotoLibrary::selectedIds() const
{
    QStringList result;
    for (const auto &e : m_visible) if (m_selection.contains(e.id)) result.append(e.id);
    return result;
}
QStringList PhotoLibrary::actionIds() const
{
    if (!m_selection.isEmpty()) return selectedIds();
    return selectedIndex()<0 ? QStringList() : QStringList{m_selected};
}
QStringList PhotoLibrary::fileFilters() const
{
    QStringList globs;
    for (const auto &ext : m_extensions) globs.append("*."+ext);
    return {tr("Photos and videos (%1)").arg(globs.join(' ')),tr("All files (*)")};
}
QVariantList PhotoLibrary::dateGroups() const
{
    QVariantList groups;
    QHash<QString,int> positions;
    for (const auto &e : m_visible) {
        const QString key=dateKey(e.date,m_dateView);
        if (!positions.contains(key)) {
            positions.insert(key,groups.size());
            groups.append(QVariantMap{{"key",key},{"label",key},{"count",0},{"coverId",e.id},
                                      {"imageSource",e.available ? "image://photos/"+e.token : QString()}});
        }
        auto group=groups.at(positions.value(key)).toMap();
        group.insert("count",group.value("count").toInt()+1); groups[positions.value(key)]=group;
    }
    return groups;
}
void PhotoLibrary::persist()
{
    // Legacy folder/favorite settings intentionally remain untouched after migration.
    m_settings.setValue("newestFirst",m_newestFirst); m_settings.sync();
    if (m_settings.status()!=QSettings::NoError) { m_message=tr("Preferences could not be saved. Your originals have not been changed."); emit changed(); }
}
void PhotoLibrary::setSearch(const QString &value) { if(m_search==value)return; m_search=value; rebuild(); }
void PhotoLibrary::setNewestFirst(bool value) { if(m_newestFirst==value)return; m_newestFirst=value; persist(); rebuild(); }
void PhotoLibrary::setDateFilter(const QString &value)
{
    if (m_dateFilter==value) return;
    if (!value.isEmpty() && !QRegularExpression("^\\d{4}(-\\d{2})?(-\\d{2})?$").match(value).hasMatch()) return;
    m_dateFilter=value; rebuild();
}
void PhotoLibrary::setMediaFilter(const QString &value)
{
    if ((value!="all" && value!="photo" && value!="video") || m_mediaFilter==value) return;
    m_mediaFilter=value; rebuild();
}
void PhotoLibrary::setDateView(const QString &value)
{
    if (value!="all" && value!="years" && value!="months" && value!="days") return;
    if(m_dateView==value)return; m_dateView=value; emit changed();
}
void PhotoLibrary::changeCollection(const QString &mode,const QUrl &folder,const QString &id)
{
    m_focusByCollection.insert(collectionKey(m_mode,m_folder,m_collectionId),m_selected);
    m_mode=mode; m_folder=folder; m_collectionId=id;
    m_selected=m_focusByCollection.value(collectionKey(mode,folder,id));
    m_search.clear(); m_dateFilter.clear(); m_mediaFilter="all";
    m_requestedSelection.clear(); m_openAfterScan=false;
    clearSelection(); rebuild(); updateWatches();
}
void PhotoLibrary::showCollection(const QString &mode,const QUrl &folder)
{
    if (mode!="all" && mode!="favorites" && mode!="folder" && mode!="session" && mode!="hidden" && mode!="recent" && mode!="trash") return;
    changeCollection(mode,folder,{});
}
void PhotoLibrary::showAlbum(const QString &id) { changeCollection("album",{},id); }
void PhotoLibrary::showMemory(const QString &id) { changeCollection("memory",{},id); }
void PhotoLibrary::addFolder(const QUrl &url)
{
    const QUrl normalized=QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absoluteFilePath());
    enqueue([url](LibraryStore &store){return store.addFolder(url);},tr("Adding folder…"));
    showCollection("folder",normalized); scheduleScan(); emit galleryRequested();
}
void PhotoLibrary::removeFolder(const QUrl &url)
{
    enqueue([url](LibraryStore &store){return store.removeFolder(url);},tr("Removing folder reference…"));
    if(m_folder==url)showCollection("all");
}
void PhotoLibrary::relinkFolder(const QUrl &oldUrl,const QUrl &newUrl)
{
    enqueue([oldUrl,newUrl](LibraryStore &store){return store.relinkFolder(oldUrl,newUrl);},tr("Updating folder location…"));
    if(m_folder==oldUrl)showCollection("folder",newUrl);
    scheduleScan();
}
void PhotoLibrary::openFiles(const QList<QUrl> &urls)
{
    if(urls.isEmpty())return;
    if(urls.size()==1 && urls.first().isLocalFile() && QFileInfo(urls.first().toLocalFile()).isDir()) {addFolder(urls.first());return;}
    m_session.clear();
    for(const auto &url:urls) if(url.isLocalFile() && m_session.size()<20000) {
        const QUrl absolute=QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absoluteFilePath());
        if(!m_session.contains(absolute))m_session.append(absolute);
    }
    if(m_session.isEmpty()){m_message=tr("Choose local photo or video files.");emit changed();return;}
    m_sessionEntries.clear(); ++m_sessionGeneration;
    showCollection("session"); m_requestedSelection=m_session.first();m_openAfterScan=true;
    scheduleScan();
}
void PhotoLibrary::select(int row)
{
    if(row<0 || row>=m_visible.size())return;
    m_selected=m_visible.at(row).id; updateWatches();emit changed();
}
void PhotoLibrary::selectById(const QString &id)
{
    for(int i=0;i<m_visible.size();++i)if(m_visible.at(i).id==id){select(i);return;}
}
void PhotoLibrary::selectionUpdated()
{
    if(!m_visible.isEmpty())emit dataChanged(index(0),index(int(m_visible.size())-1),{BatchSelectedRole});
    emit selectionChanged(); emit changed();
}
void PhotoLibrary::selectForBatch(int row,int modifiers)
{
    if(row<0 || row>=m_visible.size())return;
    const QString id=m_visible.at(row).id;
    const bool shift=modifiers & Qt::ShiftModifier, control=modifiers & Qt::ControlModifier;
    if(shift && !m_anchor.isEmpty()) {
        int anchor=-1;
        for(int i=0;i<m_visible.size();++i)if(m_visible.at(i).id==m_anchor){anchor=i;break;}
        if(!control)m_selection.clear();
        if(anchor<0)anchor=row;
        for(int i=qMin(anchor,row);i<=qMax(anchor,row);++i)m_selection.insert(m_visible.at(i).id);
    } else if(control) {
        if(m_selection.contains(id))m_selection.remove(id);else m_selection.insert(id);
        m_anchor=id;
    } else {m_selection={id};m_anchor=id;}
    m_selected=id;selectionUpdated();
}
void PhotoLibrary::selectAll()
{
    m_selection.clear();for(const auto &e:m_visible)m_selection.insert(e.id);selectionUpdated();
}
void PhotoLibrary::clearSelection(){if(m_selection.isEmpty() && m_anchor.isEmpty())return;m_selection.clear();m_anchor.clear();selectionUpdated();}
void PhotoLibrary::navigate(int delta){if(!m_visible.isEmpty())select(qBound(0,selectedIndex()+delta,int(m_visible.size())-1));}
void PhotoLibrary::toggleFavorite()
{
    const int row=selectedIndex();if(row<0)return;
    setFavorite({m_visible.at(row).id},!m_visible.at(row).favorite);
}
bool PhotoLibrary::requireLibraryIds(const QStringList &ids)
{
    for(const auto &id:ids)if(id.startsWith("session:")) {
        m_message=tr("Import temporary files before organizing them in the library.");emit changed();return false;
    }
    return true;
}
void PhotoLibrary::setFavorite(const QStringList &ids,bool value)
{
    if(!requireLibraryIds(ids))return;
    enqueue([ids,value](LibraryStore &store){return store.setFlag(ids,"favorite",value);});
}
void PhotoLibrary::setHidden(const QStringList &ids,bool value)
{
    if(!requireLibraryIds(ids))return;
    enqueue([ids,value](LibraryStore &store){return store.setFlag(ids,"hidden",value);});
}
void PhotoLibrary::updateMetadata(const QString &id,const QString &title,const QString &description,const QStringList &keywords)
{
    if(!requireLibraryIds({id}))return;
    enqueue([id,title,description,keywords](LibraryStore &store){return store.metadata(id,title,description,keywords);});
}
void PhotoLibrary::runOperation(std::function<LibrarySnapshot(LibraryStore &, const std::shared_ptr<std::atomic_bool> &, const LibraryStore::Progress &)> work,
                                const QString &label)
{
    const auto cancel=m_operationCancel;
    QPointer<PhotoLibrary> guard(this);
    enqueue([work=std::move(work),cancel,guard](LibraryStore &store){
        QElapsedTimer timer;timer.start();
        LibraryStore::Progress progress=[guard,&timer](double value,const QString &text){
            if(!guard || (value<1.0 && timer.elapsed()<80))return;
            timer.restart();
            QMetaObject::invokeMethod(guard.data(),[guard,value,text]{
                if(!guard)return;guard->m_progress=value;guard->m_operationText=text;emit guard->operationChanged();
            },Qt::QueuedConnection);
        };
        return work(store,cancel,progress);
    },label,true);
}
void PhotoLibrary::moveToTrash(const QStringList &ids)
{
    if(!requireLibraryIds(ids))return;
    runOperation([ids](LibraryStore &store,const auto &cancel,const auto &progress){return store.trash(ids,"trash",cancel,progress);},tr("Moving to Recently Deleted…"));
}
void PhotoLibrary::restore(const QStringList &ids)
{
    runOperation([ids](LibraryStore &store,const auto &cancel,const auto &progress){return store.trash(ids,"restore",cancel,progress);},tr("Restoring items…"));
}
void PhotoLibrary::permanentlyDelete(const QStringList &ids)
{
    runOperation([ids](LibraryStore &store,const auto &cancel,const auto &progress){return store.trash(ids,"purge",cancel,progress);},tr("Permanently removing items…"));
}
void PhotoLibrary::importFiles(const QList<QUrl> &urls)
{
    runOperation([urls](LibraryStore &store,const auto &cancel,const auto &progress){return store.copyFiles(urls,{}, {},true,cancel,progress);},tr("Importing originals…"));
}
void PhotoLibrary::exportOriginals(const QStringList &ids,const QUrl &destination)
{
    runOperation([ids,destination](LibraryStore &store,const auto &cancel,const auto &progress){return store.copyFiles({},ids,destination,false,cancel,progress);},tr("Exporting originals…"));
}
void PhotoLibrary::cancelOperation(){if(m_busy)m_operationCancel->store(true);}
void PhotoLibrary::createAlbum(const QString &name,const QStringList &ids)
{enqueue([name,ids](LibraryStore &store){return store.album("create",{},name,ids);});}
void PhotoLibrary::renameAlbum(const QString &id,const QString &name)
{enqueue([id,name](LibraryStore &store){return store.album("rename",id,name,{});});}
void PhotoLibrary::deleteAlbum(const QString &id)
{enqueue([id](LibraryStore &store){return store.album("delete",id,{},{});});if(m_mode=="album" && m_collectionId==id)showCollection("all");}
void PhotoLibrary::addToAlbum(const QString &id,const QStringList &ids)
{enqueue([id,ids](LibraryStore &store){return store.album("add",id,{},ids);});}
void PhotoLibrary::removeFromAlbum(const QString &id,const QStringList &ids)
{enqueue([id,ids](LibraryStore &store){return store.album("remove",id,{},ids);});}
void PhotoLibrary::setAlbumCover(const QString &id,const QString &mediaId)
{enqueue([id,mediaId](LibraryStore &store){return store.album("cover",id,{}, {mediaId});});}
void PhotoLibrary::createMemory(const QString &name,const QStringList &ids)
{enqueue([name,ids](LibraryStore &store){return store.memory("create",{},name,ids,{},4,{},true);});}
void PhotoLibrary::createDateMemory(const QString &date,const QString &name)
{
    QList<QPair<qint64,QString>> candidates;
    for(const auto &e:m_snapshot.entries)if(!e.hidden && !e.deletedAt && !e.tombstone && dateKey(e.date,"days").startsWith(date))candidates.append({e.date,e.id});
    std::sort(candidates.begin(),candidates.end());
    QStringList ids;for(const auto &candidate:candidates)ids.append(candidate.second);
    createMemory(name,ids);
}
void PhotoLibrary::updateMemory(const QString &id,const QString &name,const QStringList &ids,const QString &coverId,
                                double seconds,const QUrl &music,bool muted)
{enqueue([id,name,ids,coverId,seconds,music,muted](LibraryStore &store){return store.memory("update",id,name,ids,coverId,seconds,music,muted);});}
void PhotoLibrary::deleteMemory(const QString &id)
{enqueue([id](LibraryStore &store){return store.memory("delete",id,{}, {},{},4,{},true);});if(m_mode=="memory" && m_collectionId==id)showCollection("all");}
QVariantList PhotoLibrary::memoryItems(const QString &id) const
{
    QVariantList result;
    for(const auto &memory:m_snapshot.memories)if(memory.toMap().value("id").toString()==id)
        for(const auto &mediaId:memory.toMap().value("ids").toStringList()) {
            const auto map=entry(mediaId);if(!map.isEmpty() && !map.value("hidden").toBool() && !map.value("trashed").toBool())result.append(map);
        }
    return result;
}
void PhotoLibrary::dismissMessage(){m_message.clear();emit changed();}
void PhotoLibrary::refresh(){scheduleScan();}
void PhotoLibrary::scheduleScan()
{
    if(m_scanQueued){m_scanAgain=true;return;}
    m_scanQueued=true;
    // This serial job scans sources; changing the viewed collection does not invalidate indexing.
    const QStringList extensions=m_extensions;
    const auto cancel=m_scanCancel;
    const QList<QUrl> session=m_session;
    const quint64 generation=m_sessionGeneration;
    enqueue([extensions,cancel,session,generation](LibraryStore &store){
        auto result=store.scanSources(extensions,cancel);
        if(!session.isEmpty() && !cancel->load()) {
            const auto scanResult=LibraryStore::scan({},session,extensions,cancel);
            if(scanResult.limited)result.error=tr("Opening files paused at its time or file limit. Open a smaller selection.");
            if(!scanResult.unavailable.isEmpty())result.error=tr("Some selected files are unavailable.");
            result.sessionResult=true;result.sessionGeneration=generation;result.sessionEntries=scanResult.entries;
            for(auto &e:result.sessionEntries) {
                // Reuse a persisted ID if this explicit file is already indexed.
                for(const auto &existing:result.entries)if(existing.url==e.url){e=existing;break;}
                if(e.id.isEmpty())e.id="session:"+e.url.toString();
            }
        }
        return result;
    },{},false,true);
}
void PhotoLibrary::startScan(){scheduleScan();}
ScanResult PhotoLibrary::scan(const QStringList &directories,const QList<QUrl> &files,
                             const QStringList &extensions,const std::shared_ptr<std::atomic_bool> &cancel)
{return LibraryStore::scan(directories,files,extensions,cancel);}
void PhotoLibrary::rebuild()
{
    QList<PhotoEntry> next;
    QString sourceId;
    for(const auto &source:m_snapshot.folders)if(source.toMap().value("url").toUrl()==m_folder)sourceId=source.toMap().value("id").toString();
    QStringList memoryIds;
    if(m_mode=="memory")for(const auto &memory:m_snapshot.memories)if(memory.toMap().value("id").toString()==m_collectionId)memoryIds=memory.toMap().value("ids").toStringList();
    const auto &entries=m_mode=="session" ? m_sessionEntries : m_snapshot.entries;
    for(const auto &e:entries) {
        if(e.tombstone)continue;
        if(m_mode=="trash") {if(!e.deletedAt)continue;}
        else if(e.deletedAt)continue;
        if(m_mode=="hidden") {if(!e.hidden)continue;}
        else if(m_mode!="trash" && e.hidden)continue;
        if(m_mode=="favorites" && !e.favorite)continue;
        if(m_mode=="folder" && (sourceId.isEmpty() || !e.sourceIds.contains(sourceId)))continue;
        if(m_mode=="album" && !e.albumIds.contains(m_collectionId))continue;
        if(m_mode=="memory" && !memoryIds.contains(e.id))continue;
        if(m_mode=="recent" && e.importedAt<QDateTime::currentMSecsSinceEpoch()-qint64(30)*24*60*60*1000)continue;
        if(m_mediaFilter!="all" && e.mediaType!=m_mediaFilter)continue;
        const QString date=dateKey(e.date,"days");
        if(!m_dateFilter.isEmpty() && !date.startsWith(m_dateFilter))continue;
        const QString haystack=e.name+'\n'+e.title+'\n'+e.description+'\n'+e.keywords.join(' ')+'\n'+date;
        if(!haystack.contains(m_search,Qt::CaseInsensitive))continue;
        next.append(e);
    }
    QCollator collator;collator.setNumericMode(true);collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(next.begin(),next.end(),[&](const auto &a,const auto &b){
        if(m_mode=="memory")return memoryIds.indexOf(a.id)<memoryIds.indexOf(b.id);
        if(m_mode=="recent" && a.importedAt!=b.importedAt)return a.importedAt>b.importedAt;
        if(m_mode=="trash" && a.deletedAt!=b.deletedAt)return a.deletedAt>b.deletedAt;
        if(m_newestFirst && a.date!=b.date)return a.date>b.date;
        const int order=collator.compare(a.name,b.name);return order==0 ? a.id<b.id : order<0;
    });
    bool sameOrder=next.size()==m_visible.size();
    for(int i=0;sameOrder && i<next.size();++i)sameOrder=next.at(i).id==m_visible.at(i).id;
    if(!sameOrder){beginResetModel();m_visible=next;endResetModel();}
    else for(int i=0;i<next.size();++i)if(next.at(i)!=m_visible.at(i)){m_visible[i]=next.at(i);emit dataChanged(index(i),index(i));}
    QSet<QString> visibleIds;for(const auto &e:m_visible)visibleIds.insert(e.id);
    const auto previous=m_selection;m_selection.intersect(visibleIds);
    if(previous!=m_selection)emit selectionChanged();
    emit changed();
}
void PhotoLibrary::updateWatches()
{
    if(!m_files.directories().isEmpty())m_files.removePaths(m_files.directories());
    if(!m_files.files().isEmpty())m_files.removePaths(m_files.files());
    QStringList paths;
    for(const auto &folder:m_snapshot.folders) {
        const QString path=folder.toMap().value("path").toString();if(QFileInfo(path).isDir())paths.append(path);
    }
    const int row=selectedIndex();if(row>=0 && m_visible.at(row).available)paths.append(m_visible.at(row).source.path);
    if(!paths.isEmpty())m_files.addPaths(paths);
}
