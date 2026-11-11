// SPDX-License-Identifier: GPL-3.0-only
#include "LibraryStore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QElapsedTimer>
#include <QProcess>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

namespace {
constexpr int MaxEntries = 20000;
constexpr int MaxFolders = 64;
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString literalPath(const QUrl &url)
{
    return url.isLocalFile() ? QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath()) : QString();
}
QString json(const QStringList &list) { return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(list)).toJson(QJsonDocument::Compact)); }
QStringList stringList(const QString &text)
{
    QStringList result;
    for (const auto &value : QJsonDocument::fromJson(text.toUtf8()).array()) result.append(value.toString());
    return result;
}
QString imageUrl(const PhotoEntry &entry)
{
    return entry.available ? "image://photos/" + entry.token : QString();
}
PhotoEntry rowEntry(const QSqlQuery &query)
{
    PhotoEntry e;
    e.id = query.value("id").toString();
    e.source.path = query.value("path").toString();
    e.url = QUrl::fromLocalFile(e.source.path);
    e.name = query.value("name").toString();
    e.source.bytes = query.value("bytes").toLongLong();
    e.source.modified = query.value("modified").toLongLong();
    e.token = ImageProvider::token(e.source);
    e.mediaType = query.value("media_type").toString();
    e.date = query.value("date").toLongLong();
    e.dateSource = query.value("date_source").toString();
    e.width = query.value("width").toInt(); e.height = query.value("height").toInt();
    e.duration = query.value("duration").toLongLong();
    e.camera = query.value("camera").toString();
    e.title = query.value("title").toString(); e.description = query.value("description").toString();
    e.keywords = stringList(query.value("keywords").toString());
    e.favorite = query.value("favorite").toBool(); e.hidden = query.value("hidden").toBool();
    e.managed = query.value("managed").toBool(); e.available = query.value("available").toBool();
    e.tombstone = query.value("tombstone").toBool(); e.standalone = query.value("standalone").toBool();
    e.importedAt = query.value("imported_at").toLongLong(); e.deletedAt = query.value("deleted_at").toLongLong();
    return e;
}
QString fileIdentity(const QString &path)
{
#ifdef Q_OS_UNIX
    struct stat info;
    const QByteArray encoded=QFile::encodeName(path);
    if(::lstat(encoded.constData(),&info)!=0 || !S_ISREG(info.st_mode))return {};
    return QString::number(qulonglong(info.st_dev))+":"+QString::number(qulonglong(info.st_ino));
#else
    const QFileInfo info(path);
    return info.isFile() && !info.isSymLink() ? QString::number(info.size())+":"+QString::number(info.lastModified().toMSecsSinceEpoch()) : QString();
#endif
}
void syncDirectory(const QString &path)
{
#ifdef Q_OS_UNIX
    const QByteArray encoded=QFile::encodeName(QFileInfo(path).absolutePath());
    const int fd=::open(encoded.constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd>=0){::fsync(fd);::close(fd);}
#else
    Q_UNUSED(path);
#endif
}
QString uniqueDestination(const QString &directory, const QString &name, bool opaque)
{
    const QFileInfo info(name);
    const QString stem = info.completeBaseName().isEmpty() ? QStringLiteral("Photo") : info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();
    if (!opaque) {
        const QString candidate = QDir(directory).filePath(info.fileName());
        if (!QFileInfo::exists(candidate)) return candidate;
    }
    // Random suffixes also keep concurrent export/import batches from claiming one name.
    for (int i = 0; i < 16; ++i) {
        const QString candidate = QDir(directory).filePath(stem.left(100) + "-" + uuid() + suffix);
        if (!QFileInfo::exists(candidate)) return candidate;
    }
    return {};
}
}

LibraryStore::LibraryStore(const QString &root) : m_root(root), m_connection("nkphotos-" + uuid()) {}
LibraryStore::~LibraryStore()
{
    if (m_db.isValid()) m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}
bool LibraryStore::execute(const QString &sql, const QVariantList &values)
{
    QSqlQuery query(m_db);
    if (!query.prepare(sql)) { m_error = query.lastError().text(); return false; }
    for (const auto &value : values)
        query.addBindValue(value.metaType().id() == QMetaType::QString && value.toString().isNull() ? QVariant(QStringLiteral("")) : value);
    if (!query.exec()) { m_error = query.lastError().text(); return false; }
    return true;
}
bool LibraryStore::begin()
{
    if (m_db.transaction()) return true;
    m_error = m_db.lastError().text(); return false;
}
bool LibraryStore::commit()
{
    if (m_db.commit()) return true;
    m_error = m_db.lastError().text(); m_db.rollback(); return false;
}
void LibraryStore::rollback() { m_db.rollback(); }
bool LibraryStore::open(const QStringList &legacyFolders, const QStringList &legacyFavorites)
{
    if (!QDir().mkpath(m_root + "/originals") || !QDir().mkpath(m_root + "/recovery")) {
        m_error = QObject::tr("The library data directory could not be created."); return false;
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(m_root + "/library.sqlite3");
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    if (!m_db.open()) { m_error = m_db.lastError().text(); return false; }
    if (!execute("PRAGMA foreign_keys=ON") || !execute("PRAGMA journal_mode=WAL")
        || !execute("PRAGMA synchronous=FULL")) return false;
    QSqlQuery version(m_db);
    if (!version.exec("PRAGMA user_version") || !version.next()) { m_error = version.lastError().text(); return false; }
    if (version.value(0).toInt() > 1) { m_error = QObject::tr("This library was created by a newer version of Photos."); return false; }
    if(version.value(0).toInt()==1) {
        version.finish();
        QSqlQuery migrated(m_db);
        if(migrated.exec("SELECT value FROM settings WHERE key='legacy_migrated'") && migrated.next()) {
            m_ready=true;return true;
        }
    }
    version.finish();
    if (!begin()) return false;
    const QStringList schema = {
        "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS media(id TEXT PRIMARY KEY,path TEXT NOT NULL UNIQUE,name TEXT NOT NULL,"
        "media_type TEXT NOT NULL DEFAULT 'photo',bytes INTEGER NOT NULL DEFAULT 0,modified INTEGER NOT NULL DEFAULT 0,"
        "date INTEGER NOT NULL DEFAULT 0,date_source TEXT NOT NULL DEFAULT 'file',width INTEGER NOT NULL DEFAULT 0,"
        "height INTEGER NOT NULL DEFAULT 0,duration INTEGER NOT NULL DEFAULT 0,camera TEXT NOT NULL DEFAULT '',"
        "title TEXT NOT NULL DEFAULT '',description TEXT NOT NULL DEFAULT '',keywords TEXT NOT NULL DEFAULT '[]',"
        "favorite INTEGER NOT NULL DEFAULT 0,hidden INTEGER NOT NULL DEFAULT 0,managed INTEGER NOT NULL DEFAULT 0,"
        "available INTEGER NOT NULL DEFAULT 1,standalone INTEGER NOT NULL DEFAULT 0,imported_at INTEGER NOT NULL DEFAULT 0,"
        "deleted_at INTEGER NOT NULL DEFAULT 0,tombstone INTEGER NOT NULL DEFAULT 0,original_path TEXT NOT NULL DEFAULT '',batch_id TEXT NOT NULL DEFAULT '')",
        "CREATE TABLE IF NOT EXISTS sources(id TEXT PRIMARY KEY,path TEXT NOT NULL UNIQUE,available INTEGER NOT NULL DEFAULT 1)",
        "CREATE TABLE IF NOT EXISTS media_sources(media_id TEXT NOT NULL REFERENCES media(id),source_id TEXT NOT NULL REFERENCES sources(id) ON DELETE CASCADE,relative_path TEXT NOT NULL,PRIMARY KEY(media_id,source_id))",
        "CREATE TABLE IF NOT EXISTS albums(id TEXT PRIMARY KEY,name TEXT NOT NULL,cover_id TEXT NOT NULL DEFAULT '')",
        "CREATE TABLE IF NOT EXISTS album_members(album_id TEXT NOT NULL REFERENCES albums(id) ON DELETE CASCADE,media_id TEXT NOT NULL REFERENCES media(id),position INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(album_id,media_id))",
        "CREATE TABLE IF NOT EXISTS memories(id TEXT PRIMARY KEY,name TEXT NOT NULL,cover_id TEXT NOT NULL DEFAULT '',seconds REAL NOT NULL DEFAULT 4,music TEXT NOT NULL DEFAULT '',muted INTEGER NOT NULL DEFAULT 1,created_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS memory_members(memory_id TEXT NOT NULL REFERENCES memories(id) ON DELETE CASCADE,media_id TEXT NOT NULL REFERENCES media(id),position INTEGER NOT NULL,PRIMARY KEY(memory_id,media_id))",
        "CREATE TABLE IF NOT EXISTS operations(id TEXT PRIMARY KEY,type TEXT NOT NULL,media_id TEXT NOT NULL DEFAULT '',src TEXT NOT NULL DEFAULT '',dst TEXT NOT NULL DEFAULT '',temporary TEXT NOT NULL DEFAULT '',state TEXT NOT NULL DEFAULT 'prepared',payload TEXT NOT NULL DEFAULT '{}',error TEXT NOT NULL DEFAULT '',created_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS owned_files(media_id TEXT PRIMARY KEY REFERENCES media(id),identity TEXT NOT NULL)",
        "CREATE INDEX IF NOT EXISTS media_date ON media(date)",
        "CREATE INDEX IF NOT EXISTS media_deleted ON media(deleted_at)",
        "CREATE INDEX IF NOT EXISTS media_sources_source ON media_sources(source_id)"
    };
    for (const auto &sql : schema) if (!execute(sql)) { rollback(); return false; }
    QSqlQuery migration(m_db);
    migration.exec("SELECT value FROM settings WHERE key='legacy_migrated'");
    const bool migrated = migration.next();
    if (!migrated) {
        for (const auto &value : legacyFolders.mid(0, MaxFolders)) {
            const QString path = literalPath(QUrl(value));
            if (path.isEmpty()) continue;
            if (!execute("INSERT OR IGNORE INTO sources(id,path,available) VALUES(?,?,?)",
                         {uuid(), path, QFileInfo(path).isReadable()})) { rollback(); return false; }
        }
        // Keep inaccessible and out-of-folder favorites. Migration never depends on a successful scan.
        for (const auto &value : legacyFavorites.mid(0, MaxEntries)) {
            const QString path = literalPath(QUrl(value));
            if (path.isEmpty()) continue;
            PhotoEntry e = inspect(path);
            e.id = idForPath(path); if (e.id.isEmpty()) e.id = uuid();
            e.favorite = true; e.standalone = true;
            if (!saveEntry(e) || !execute("UPDATE media SET favorite=1,standalone=1 WHERE id=?", {e.id})) { rollback(); return false; }
        }
        if (!execute("INSERT INTO settings(key,value) VALUES('legacy_migrated','1')")) { rollback(); return false; }
    }
    if (!execute("PRAGMA user_version=1")) { rollback(); return false; }
    m_ready = commit();
    return m_ready;
}

PhotoEntry LibraryStore::inspect(const QString &path)
{
    const QFileInfo info(path);
    PhotoEntry e;
    e.source = {info.absoluteFilePath(), info.size(), info.lastModified().toMSecsSinceEpoch()};
    e.url = QUrl::fromLocalFile(e.source.path); e.name = info.fileName();
    e.token = ImageProvider::token(e.source);
    e.available = info.isFile() && info.isReadable() && !info.isSymLink();
    e.date = e.source.modified;
    const QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
    e.mediaType = mime.startsWith("video/") ? "video" : "photo";
    if (e.available && e.mediaType == "photo") {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        e.width = qMax(0, size.width()); e.height = qMax(0, size.height());
        // Use metadata exposed by installed Qt image plugins; never invent capture dates.
        for (const QString &key : {QStringLiteral("DateTimeOriginal"), QStringLiteral("DateTimeDigitized"),
                                    QStringLiteral("Creation Time"), QStringLiteral("CreationTime")}) {
            const QString text = reader.text(key);
            QDateTime date = QDateTime::fromString(text, Qt::ISODate);
            if (!date.isValid()) date = QDateTime::fromString(text, "yyyy:MM:dd HH:mm:ss");
            if (date.isValid()) { e.date = date.toMSecsSinceEpoch(); e.dateSource = "capture"; break; }
        }
        e.camera = (reader.text("Make") + " " + reader.text("Model")).simplified();
    } else if (e.available && e.mediaType == "video") {
        QProcess probe;
        probe.setStandardErrorFile(QProcess::nullDevice());
        probe.start("ffprobe", {"-v","error","-protocol_whitelist","file,pipe","-select_streams","v:0",
                               "-show_entries","stream=width,height,duration:stream_tags=creation_time:format=duration:format_tags=creation_time",
                               "-of","json",e.source.path},QIODevice::ReadOnly);
        QByteArray output;
        QElapsedTimer timer;timer.start();
        bool completed=false;
        if(probe.waitForStarted(500)) {
            while(timer.elapsed()<3000 && output.size()<=64*1024) {
                completed=probe.waitForFinished(50);output+=probe.readAllStandardOutput();
                if(completed)break;
            }
        }
        if(!completed){probe.kill();probe.waitForFinished(500);}
        if(completed && probe.exitStatus()==QProcess::NormalExit && probe.exitCode()==0 && output.size()<=64*1024) {
            const auto document=QJsonDocument::fromJson(output).object();
            const auto streams=document.value("streams").toArray();
            const auto stream=streams.isEmpty() ? QJsonObject() : streams.first().toObject();
            const auto format=document.value("format").toObject();
            e.width=qMax(0,stream.value("width").toInt());e.height=qMax(0,stream.value("height").toInt());
            bool durationOk=false;
            double seconds=format.value("duration").toString().toDouble(&durationOk);
            if(!durationOk)seconds=stream.value("duration").toString().toDouble(&durationOk);
            if(durationOk && seconds>0 && seconds<365.0*24*60*60)e.duration=qRound64(seconds*1000);
            QString capture=stream.value("tags").toObject().value("creation_time").toString();
            if(capture.isEmpty())capture=format.value("tags").toObject().value("creation_time").toString();
            const auto date=QDateTime::fromString(capture,Qt::ISODate);
            if(date.isValid()){e.date=date.toMSecsSinceEpoch();e.dateSource="capture";}
        }
    }
    return e;
}
bool LibraryStore::saveEntry(const PhotoEntry &e)
{
    return execute("INSERT INTO media(id,path,name,media_type,bytes,modified,date,date_source,width,height,duration,camera,"
                   "title,description,keywords,favorite,hidden,managed,available,standalone,imported_at,deleted_at,tombstone) "
                   "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
                   "ON CONFLICT(id) DO UPDATE SET path=excluded.path,name=excluded.name,media_type=excluded.media_type,"
                   "bytes=excluded.bytes,modified=excluded.modified,date=excluded.date,date_source=excluded.date_source,"
                   "width=excluded.width,height=excluded.height,duration=excluded.duration,camera=excluded.camera,available=excluded.available",
                   {e.id,e.source.path,e.name,e.mediaType,e.source.bytes,e.source.modified,e.date,e.dateSource,
                    e.width,e.height,e.duration,e.camera,e.title,e.description,json(e.keywords),e.favorite,e.hidden,
                    e.managed,e.available,e.standalone,e.importedAt,e.deletedAt,e.tombstone});
}
QString LibraryStore::idForPath(const QString &path)
{
    QSqlQuery q(m_db); q.prepare("SELECT id FROM media WHERE path=?"); q.addBindValue(path);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}
PhotoEntry LibraryStore::find(const QString &id)
{
    QSqlQuery q(m_db); q.prepare("SELECT * FROM media WHERE id=?"); q.addBindValue(id);
    return q.exec() && q.next() ? rowEntry(q) : PhotoEntry();
}
LibrarySnapshot LibraryStore::snapshot()
{
    LibrarySnapshot result; result.error = m_error; result.results = m_results; result.ready = m_ready;
    if (!m_ready) return result;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT * FROM media WHERE tombstone=0 AND (standalone=1 OR managed=1 OR deleted_at>0 OR EXISTS(SELECT 1 FROM media_sources WHERE media_id=media.id))")) {
        result.error = q.lastError().text(); return result;
    }
    QHash<QString, int> positions;
    while (q.next()) { PhotoEntry e = rowEntry(q); positions.insert(e.id, result.entries.size()); result.entries.append(e); }
    q.exec("SELECT media_id,source_id FROM media_sources");
    while (q.next()) if (positions.contains(q.value(0).toString())) result.entries[positions.value(q.value(0).toString())].sourceIds.append(q.value(1).toString());
    q.exec("SELECT media_id,album_id FROM album_members");
    while (q.next()) if (positions.contains(q.value(0).toString())) result.entries[positions.value(q.value(0).toString())].albumIds.append(q.value(1).toString());
    q.exec("SELECT id,path,available FROM sources ORDER BY rowid");
    while (q.next()) {
        const QString path = q.value(1).toString();
        result.folders.append(QVariantMap{{"id",q.value(0)}, {"url",QUrl::fromLocalFile(path)}, {"path",path},
                                         {"name",QFileInfo(path).fileName()}, {"available",q.value(2).toBool()}});
    }
    QSqlQuery pending(m_db);
    if(result.results.isEmpty() && pending.exec("SELECT media_id,src,state,error FROM operations WHERE state IN ('failed','canceled','ready','prepared') ORDER BY created_at DESC LIMIT 100")) {
        while(pending.next())result.results.append(QVariantMap{{"id",pending.value(0)},
            {"url",QUrl::fromLocalFile(pending.value(1).toString())},{"status",pending.value(2)},{"error",pending.value(3)}});
    }
    auto collection = [&](bool memory) {
        QVariantList list;
        QSqlQuery collections(m_db);
        collections.exec(memory ? "SELECT * FROM memories ORDER BY created_at DESC" : "SELECT * FROM albums ORDER BY name COLLATE NOCASE");
        while (collections.next()) {
            const QString id = collections.value("id").toString();
            QString cover = collections.value("cover_id").toString();
            QStringList ids;
            QSqlQuery members(m_db);
            members.prepare(memory ? "SELECT media_id FROM memory_members WHERE memory_id=? ORDER BY position" : "SELECT media_id FROM album_members WHERE album_id=? ORDER BY position");
            members.addBindValue(id); members.exec();
            while (members.next()) {
                const QString mediaId = members.value(0).toString();
                if (!positions.contains(mediaId)) continue;
                const auto &entry = result.entries.at(positions.value(mediaId));
                if (entry.hidden || entry.deletedAt || entry.tombstone) continue;
                ids.append(mediaId);
            }
            if (!ids.contains(cover)) cover = ids.value(0);
            const QString image = positions.contains(cover) ? imageUrl(result.entries.at(positions.value(cover))) : QString();
            QVariantMap map{{"id",id},{"name",collections.value("name")},{"coverId",cover},
                            {"imageSource",image},{"count",ids.size()},{"ids",ids}};
            if (memory) {
                map.insert("seconds",collections.value("seconds"));
                map.insert("music",QUrl(collections.value("music").toString()));
                map.insert("muted",collections.value("muted").toBool());
            }
            list.append(map);
        }
        return list;
    };
    result.albums = collection(false); result.memories = collection(true);
    return result;
}

ScanResult LibraryStore::scan(const QStringList &directories, const QList<QUrl> &files,
                              const QStringList &extensions, const std::shared_ptr<std::atomic_bool> &cancel)
{
    ScanResult result;
    QSet<QString> known;
    auto append = [&](const QFileInfo &info, bool explicitFile) {
        if (cancel->load() || info.isSymLink()) return;
        if (!explicitFile && !extensions.contains(info.suffix().toLower())) return;
        if (known.contains(info.absoluteFilePath())) return;
        if (result.entries.size() >= MaxEntries) { result.limited = true; return; }
        if (!info.isFile() || !info.isReadable()) { result.unavailable.append(info.absoluteFilePath()); return; }
        known.insert(info.absoluteFilePath());
        result.entries.append(inspect(info.absoluteFilePath()));
    };
    QElapsedTimer scanBudget;scanBudget.start();
    for (const auto &url : files) {
        if(scanBudget.elapsed()>10000){result.limited=true;break;}
        if (cancel->load() || result.limited) break;
        if (url.isLocalFile()) append(QFileInfo(url.toLocalFile()), true);
    }
    for (const auto &path : directories) {
        if (cancel->load() || result.limited) break;
        if (!QFileInfo(path).isDir() || !QFileInfo(path).isReadable()) { result.unavailable.append(path); continue; }
        QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::NoIteratorFlags);
        while (it.hasNext() && !cancel->load() && !result.limited) {
            if(scanBudget.elapsed()>10000){result.limited=true;break;}
            it.next(); append(it.fileInfo(), false);
        }
    }
    return result;
}
LibrarySnapshot LibraryStore::scanSources(const QStringList &extensions,
                                          const std::shared_ptr<std::atomic_bool> &cancel)
{
    struct Source { QString id, path; };
    QList<Source> sources;
    QSqlQuery roots(m_db); roots.exec("SELECT id,path FROM sources");
    while (roots.next()) sources.append({roots.value(0).toString(), roots.value(1).toString()});
    QHash<QString,PhotoEntry> known;
    QSqlQuery indexed(m_db);indexed.exec("SELECT * FROM media");
    while(indexed.next()){PhotoEntry e=rowEntry(indexed);known.insert(e.source.path,e);}
    QElapsedTimer scanBudget;scanBudget.start();
    int discovered = 0;
    for (const auto &source : sources) {
        if (cancel->load()) break;
        ScanResult result;
        const QFileInfo directory(source.path);
        if(!directory.isDir() || !directory.isReadable())result.unavailable.append(source.path);
        else {
            QDirIterator iterator(source.path,QDir::Files|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDirIterator::NoIteratorFlags);
            while(iterator.hasNext() && !cancel->load()) {
                iterator.next();const QFileInfo info=iterator.fileInfo();
                if(!extensions.contains(info.suffix().toLower()))continue;
                if(discovered+result.entries.size()>=MaxEntries || scanBudget.elapsed()>10000){result.limited=true;break;}
                if(!info.isReadable()){result.unavailable.append(info.absoluteFilePath());continue;}
                PhotoEntry e=known.value(info.absoluteFilePath());
                if(e.id.isEmpty() || !e.available || e.source.bytes!=info.size() || e.source.modified!=info.lastModified().toMSecsSinceEpoch())
                    e=inspect(info.absoluteFilePath());
                result.entries.append(e);
            }
        }
        // A canceled/truncated/inaccessible scan never makes conclusions about absent records.
        if (cancel->load()) break;
        if (!begin()) break;
        if (cancel->load()) { rollback(); break; }
        bool ok = execute("UPDATE sources SET available=? WHERE id=?", {QFileInfo(source.path).isReadable(), source.id});
        for (auto e : result.entries) {
            if (cancel->load() || discovered >= MaxEntries) break;
            e.id = idForPath(e.source.path);
            if (e.id.isEmpty()) { e.id = uuid(); e.importedAt = QDateTime::currentMSecsSinceEpoch(); }
            ok = ok && saveEntry(e)
                && execute("INSERT OR IGNORE INTO media_sources(media_id,source_id,relative_path) VALUES(?,?,?)",
                           {e.id,source.id,QDir(source.path).relativeFilePath(e.source.path)});
            ++discovered;
            if (!ok) break;
        }
        if (!ok || cancel->load()) { rollback(); if (!ok) break; }
        else if (!commit()) break;
        if (result.limited || discovered >= MaxEntries) { m_error = QObject::tr("This refresh reached its 20,000-file or 10-second indexing limit. Choose smaller source folders to include omitted files."); break; }
        if (!result.unavailable.isEmpty()) m_error = QObject::tr("Some files or folders are unavailable. Reopen their location to grant access again.");
    }
    // Verify each known locator directly. Never infer absence from the contents of a scan.
    if (!cancel->load()) {
        QSqlQuery all(m_db); all.exec("SELECT id,path FROM media WHERE tombstone=0");
        QList<QPair<QString,bool>> availability;
        while (all.next() && !cancel->load()) {
            const QFileInfo info(all.value(1).toString());
            availability.append({all.value(0).toString(), info.isFile() && info.isReadable() && !info.isSymLink()});
        }
        if (!cancel->load() && begin()) {
            bool ok = true;
            for (const auto &item : availability) ok = ok && execute("UPDATE media SET available=? WHERE id=?", {item.second,item.first});
            if (ok && !cancel->load()) commit(); else rollback();
        }
    }
    if(!cancel->load() && m_error.isEmpty())generateDateMemories(cancel);
    return snapshot();
}
void LibraryStore::generateDateMemories(const std::shared_ptr<std::atomic_bool> &cancel)
{
    // Materialize bounded, local date groups only after a complete successful source scan.
    // A marker and the memory are committed together; deleting the memory keeps the marker.
    QSqlQuery candidates(m_db);
    if(!candidates.exec("SELECT id,date FROM media WHERE media_type='photo' AND hidden=0 AND deleted_at=0 "
                        "AND tombstone=0 AND available=1 AND date>0 AND (managed=1 OR standalone=1 OR "
                        "EXISTS(SELECT 1 FROM media_sources WHERE media_id=media.id)) ORDER BY date DESC,id LIMIT 20000")) {
        m_error=candidates.lastError().text();return;
    }
    QHash<QString,QStringList> groups;
    QStringList dates;
    while(candidates.next() && !cancel->load()) {
        const QString date=QDateTime::fromMSecsSinceEpoch(candidates.value(1).toLongLong()).date().toString(Qt::ISODate);
        if(!groups.contains(date))dates.append(date);
        // Keep each automatic memory short; the full date collection remains separately available.
        auto &ids=groups[date];if(ids.size()<200)ids.append(candidates.value(0).toString());
    }
    candidates.finish();
    if(cancel->load() || !begin())return;
    bool ok=true;
    int considered=0;
    for(const auto &date:dates) {
        QStringList ids=groups.value(date);
        if(ids.size()<3)continue;
        if(++considered>12 || cancel->load())break;
        const QString marker="auto_date_memory/"+date;
        QSqlQuery existing(m_db);existing.prepare("SELECT 1 FROM settings WHERE key=?");existing.addBindValue(marker);
        if(!existing.exec()){m_error=existing.lastError().text();ok=false;break;}
        const bool generated=existing.next();existing.finish();
        if(generated)continue;
        std::reverse(ids.begin(),ids.end());
        const QString id=uuid();
        ok=execute("INSERT INTO memories(id,name,cover_id,seconds,music,muted,created_at) VALUES(?,?,?,4,'',1,?)",
                   {id,date,ids.first(),QDateTime::currentMSecsSinceEpoch()});
        int position=0;
        for(const auto &mediaId:ids) {
            if(!ok || cancel->load())break;
            ok=execute("INSERT INTO memory_members(memory_id,media_id,position) VALUES(?,?,?)",{id,mediaId,position++});
        }
        ok=ok && execute("INSERT INTO settings(key,value) VALUES(?,?)",{marker,id});
        if(!ok)break;
    }
    if(ok && !cancel->load())commit();else rollback();
}
LibrarySnapshot LibraryStore::addFolder(const QUrl &url)
{
    const QString path = literalPath(url);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isDir() || !info.isReadable() || info.isSymLink()) {
        m_error = QObject::tr("This folder is not accessible. Choose it again using the folder picker."); return snapshot();
    }
    QSqlQuery count(m_db); count.exec("SELECT COUNT(*) FROM sources"); count.next();
    QSqlQuery existing(m_db); existing.prepare("SELECT id FROM sources WHERE path=?"); existing.addBindValue(path); existing.exec();
    const bool exists = existing.next();
    if (!exists && count.value(0).toInt() >= MaxFolders) { m_error = QObject::tr("You can add up to 64 folders."); return snapshot(); }
    if (!begin()) return snapshot();
    bool ok = execute("INSERT INTO sources(id,path) VALUES(?,?) ON CONFLICT(path) DO UPDATE SET available=1", {uuid(),path});
    // Explicitly adding a source is the only scan-related action that lifts its exclusion tombstones.
    ok = ok && execute("UPDATE media SET tombstone=0,deleted_at=0 WHERE managed=0 AND path IN "
                       "(SELECT m.path FROM media m WHERE m.tombstone=1 AND substr(m.path,1,?)=? AND instr(substr(m.path,?),'/' )=0)",
                       {path.size()+1,path+"/",path.size()+2});
    if (ok) commit(); else rollback();
    return snapshot();
}
LibrarySnapshot LibraryStore::removeFolder(const QUrl &url)
{
    execute("DELETE FROM sources WHERE path=?", {literalPath(url)});
    return snapshot();
}
LibrarySnapshot LibraryStore::relinkFolder(const QUrl &oldUrl, const QUrl &newUrl)
{
    const QString oldPath = literalPath(oldUrl), newPath = literalPath(newUrl);
    if (newPath.isEmpty() || !QFileInfo(newPath).isDir() || !QFileInfo(newPath).isReadable()) {
        m_error = QObject::tr("The replacement folder is not accessible."); return snapshot();
    }
    QSqlQuery source(m_db); source.prepare("SELECT id FROM sources WHERE path=?"); source.addBindValue(oldPath); source.exec();
    if (!source.next()) { m_error = QObject::tr("The original source folder no longer exists in the library."); return snapshot(); }
    const QString id = source.value(0).toString();
    if (!begin()) return snapshot();
    bool ok = execute("UPDATE sources SET path=?,available=1 WHERE id=?", {newPath,id});
    QSqlQuery members(m_db); members.prepare("SELECT media_id,relative_path FROM media_sources WHERE source_id=?"); members.addBindValue(id); members.exec();
    while (ok && members.next()) {
        const QString path = QDir(newPath).filePath(members.value(1).toString());
        const QFileInfo info(path);
        ok = execute("UPDATE media SET path=?,available=? WHERE id=? AND managed=0", {path,info.isFile() && info.isReadable(),members.value(0)});
    }
    if (ok) commit(); else rollback();
    return snapshot();
}
LibrarySnapshot LibraryStore::setFlag(const QStringList &ids, const QString &field, bool value)
{
    if (field != "favorite" && field != "hidden") return snapshot();
    if (!begin()) return snapshot();
    bool ok = true;
    for (const auto &id : ids) ok = ok && execute("UPDATE media SET " + field + "=? WHERE id=? AND tombstone=0 AND deleted_at=0", {value,id});
    if (ok) commit(); else rollback();
    return snapshot();
}

bool LibraryStore::ownedPath(const QString &path) const
{
    const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString originals = QDir::cleanPath(m_root + "/originals") + "/";
    const QString recovery = QDir::cleanPath(m_root + "/recovery") + "/";
    if (!absolute.startsWith(originals) && !absolute.startsWith(recovery)) return false;
    const QFileInfo info(absolute);
    if (info.isSymLink()) return false;
    // Reject a swapped/symlinked parent even for a file that has already gone missing.
    const QString parent = QFileInfo(info.absolutePath()).canonicalFilePath();
    const QString expected = absolute.startsWith(originals)
        ? QFileInfo(m_root + "/originals").absoluteFilePath()
        : QFileInfo(m_root + "/recovery").absoluteFilePath();
    return !parent.isEmpty() && parent == expected && !QFileInfo(expected).isSymLink();
}
void LibraryStore::recordResult(const QString &id, const QUrl &url, const QString &status, const QString &error)
{
    m_results.append(QVariantMap{{"id",id},{"url",url},{"status",status},{"error",error}});
}
QString LibraryStore::journal(const QString &type, const QString &id, const QString &src,
                              const QString &dst, const QString &temporary, const QVariantMap &payload)
{
    const QString operation = uuid();
    if (!execute("INSERT INTO operations(id,type,media_id,src,dst,temporary,payload,created_at) VALUES(?,?,?,?,?,?,?,?)",
                 {operation,type,id,src,dst,temporary,
                  QString::fromUtf8(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact)),
                  QDateTime::currentMSecsSinceEpoch()})) return {};
    return operation;
}
void LibraryStore::failOperation(const QString &operation, const QString &error, const QString &state)
{
    execute("UPDATE operations SET state=?,error=? WHERE id=?", {state,error,operation});
}
bool LibraryStore::finalizeOperation(const QString &operation, const std::shared_ptr<std::atomic_bool> &cancel)
{
    QSqlQuery query(m_db); query.prepare("SELECT * FROM operations WHERE id=?"); query.addBindValue(operation);
    if (!query.exec() || !query.next()) return false;
    if (query.value("state").toString() == "done") return true;
    if (query.value("state").toString() != "ready") return false;
    const QString type = query.value("type").toString(), id = query.value("media_id").toString();
    const QString src = query.value("src").toString(), dst = query.value("dst").toString();
    const QString temporary = query.value("temporary").toString();
    const QVariantMap payload = QJsonDocument::fromJson(query.value("payload").toByteArray()).toVariant().toMap();
    auto fail = [&](const QString &error) { m_error = error; failOperation(operation,error); return false; };
    if (type == "import" || type == "export") {
        if (type == "import" && (!ownedPath(dst) || !ownedPath(temporary))) return fail(QObject::tr("The managed destination is no longer safe to use."));
        const QString expectedIdentity=payload.value("temporaryIdentity").toString();
        if(expectedIdentity.isEmpty())return fail(QObject::tr("The completed copy has no verified file identity."));
        if (!QFileInfo::exists(dst)) {
            if (fileIdentity(temporary)!=expectedIdentity || !QFile::rename(temporary,dst))
                return fail(QObject::tr("The completed copy could not be placed at its destination."));
        } else if(fileIdentity(dst)!=expectedIdentity) {
            return fail(QObject::tr("The destination changed before the copy could be recorded. It was not overwritten."));
        }
        // Crash recovery must not adopt an unrelated file which appeared under the requested name.
        QFile verification(dst);
        QCryptographicHash digest(QCryptographicHash::Sha256);
        if(!verification.open(QIODevice::ReadOnly))return fail(verification.errorString());
        QByteArray verifyBuffer(1024*1024,Qt::Uninitialized);
        while(!verification.atEnd()) {
            if(cancel && cancel->load()) {
                verification.close();
                if(fileIdentity(dst)==expectedIdentity && QFile::remove(dst)) {
                    syncDirectory(dst);
                    m_error=QObject::tr("The copy was canceled. The original was kept.");
                    failOperation(operation,m_error,"canceled");return false;
                }
                return fail(QObject::tr("Cancellation could not remove the completed copy. Its destination was kept for recovery."));
            }
            const qint64 bytes=verification.read(verifyBuffer.data(),verifyBuffer.size());
            if(bytes<0)return fail(verification.errorString());
            if(!bytes)break;
            digest.addData(QByteArrayView(verifyBuffer.constData(),bytes));
        }
        verification.close();
        if(QString::fromLatin1(digest.result().toHex())!=payload.value("sha256").toString())
            return fail(QObject::tr("The destination changed before the copy could be recorded. It was not overwritten."));
        syncDirectory(dst);
        PhotoEntry imported;
        if(type=="import") {
            imported=inspect(dst);imported.id=id;imported.managed=true;
            imported.name=QFileInfo(src).fileName();
            imported.importedAt=payload.value("importedAt").toLongLong();
        }
        if (!begin()) return false;
        bool ok = true;
        if (type == "import") {
            ok = saveEntry(imported) && execute("UPDATE media SET batch_id=?,original_path=? WHERE id=?", {payload.value("batch"),src,id})
                 && execute("INSERT OR REPLACE INTO owned_files(media_id,identity) VALUES(?,?)",{id,fileIdentity(dst)});
        }
        ok = ok && execute("UPDATE operations SET state='done',error='' WHERE id=?", {operation});
        if (!ok) { rollback(); return false; }
        return commit();
    }
    PhotoEntry e = find(id);
    if (e.id.isEmpty()) return fail(QObject::tr("The library item no longer exists."));
    if(e.managed) {
        QSqlQuery ownership(m_db);ownership.prepare("SELECT identity FROM owned_files WHERE media_id=?");ownership.addBindValue(id);ownership.exec();
        const QString expected=ownership.next() ? ownership.value(0).toString() : QString();
        const QString actualPath=QFileInfo::exists(src) ? src : ((type=="trash" || type=="restore") ? dst : src);
        if(QFileInfo::exists(actualPath) && (expected.isEmpty() || fileIdentity(actualPath)!=expected))
            return fail(QObject::tr("The managed file was replaced outside Photos. It was kept untouched."));
    }
    if (type == "trash" || type == "restore") {
        if (e.managed) {
            if (!ownedPath(src) || !ownedPath(dst)) return fail(QObject::tr("Only files owned by this library can be moved."));
            if (QFileInfo::exists(src)) {
                if (QFileInfo::exists(dst) || !QFile::rename(src,dst)) return fail(QObject::tr("The managed file could not be moved. Its current copy was kept."));
            } else if (!QFileInfo::exists(dst)) return fail(QObject::tr("The managed file is unavailable. Restore access before trying again."));
            syncDirectory(src);syncDirectory(dst);
        }
        if (!begin()) return false;
        const bool ok = execute("UPDATE media SET path=?,deleted_at=?,available=?,original_path=CASE WHEN ?='trash' THEN ? ELSE original_path END WHERE id=?",
                                {e.managed ? dst : e.source.path, type == "trash" ? payload.value("deletedAt").toLongLong() : qint64(0),
                                 QFileInfo(e.managed ? dst : e.source.path).isReadable(),type,src,id})
                     && execute("UPDATE operations SET state='done',error='' WHERE id=?", {operation});
        if (!ok) { rollback(); return false; }
        return commit();
    }
    if (type == "purge") {
        if (e.managed) {
            if (!ownedPath(src)) return fail(QObject::tr("Only files owned by this library can be permanently deleted."));
            if (QFileInfo::exists(src) && !QFile::remove(src)) return fail(QObject::tr("The managed file could not be deleted. You can retry from Recently Deleted."));
            syncDirectory(src);
        }
        if (!begin()) return false;
        const bool ok = execute("UPDATE media SET tombstone=1,available=0 WHERE id=?", {id})
                     && execute("DELETE FROM album_members WHERE media_id=?", {id})
                     && execute("DELETE FROM memory_members WHERE media_id=?", {id})
                     && execute("UPDATE operations SET state='done',error='' WHERE id=?", {operation});
        if (!ok) { rollback(); return false; }
        return commit();
    }
    return fail(QObject::tr("Unknown pending library operation."));
}
void LibraryStore::recover()
{
    if (!m_ready) return;
    QSqlQuery query(m_db);
    query.exec("SELECT id,type,temporary,state,payload FROM operations WHERE state IN ('prepared','ready') ORDER BY created_at LIMIT 100");
    struct Pending { QString id,type,temporary,state,identity; };
    QList<Pending> pending;
    while (query.next()) pending.append({query.value(0).toString(),query.value(1).toString(),query.value(2).toString(),query.value(3).toString(),
        QJsonDocument::fromJson(query.value(4).toByteArray()).object().value("temporaryIdentity").toString()});
    for (const auto &item : pending) {
        if (item.state == "ready") {
            if (finalizeOperation(item.id)) recordResult({}, {}, "recovered", QObject::tr("A pending file operation was completed."));
            else recordResult({}, {}, "failed", m_error);
        } else {
            // Only remove our unique unfinished copy, never the source or final destination.
            if (!item.temporary.isEmpty() && !item.identity.isEmpty()
                && fileIdentity(item.temporary)==item.identity && QFileInfo(item.temporary).fileName().startsWith(".nkphotos-"))
                QFile::remove(item.temporary);
            const QString error = QObject::tr("An interrupted copy was canceled. The original was kept; you can retry the operation.");
            failOperation(item.id,error,"canceled"); recordResult({}, {}, "canceled",error);
        }
    }
    // Retention is bounded and only runs while Photos is open. Referenced originals are never removed.
    QStringList expired;
    QSqlQuery expiry(m_db); expiry.prepare("SELECT id FROM media WHERE tombstone=0 AND deleted_at>0 AND deleted_at<? LIMIT 100");
    expiry.addBindValue(QDateTime::currentMSecsSinceEpoch()-qint64(30)*24*60*60*1000); expiry.exec();
    while (expiry.next()) expired.append(expiry.value(0).toString());
    if (!expired.isEmpty()) trash(expired,"purge",std::make_shared<std::atomic_bool>(false),{});
}
LibrarySnapshot LibraryStore::trash(const QStringList &ids, const QString &action,
                                    const std::shared_ptr<std::atomic_bool> &cancel, const Progress &progress)
{
    if (action != "trash" && action != "restore" && action != "purge") return snapshot();
    int done = 0;
    for (const auto &id : ids) {
        const PhotoEntry e = find(id);
        if (cancel->load()) { recordResult(id,e.url,"canceled"); continue; }
        if (e.id.isEmpty() || e.tombstone || (action == "trash" && e.deletedAt)
            || ((action == "restore" || action == "purge") && !e.deletedAt)) {
            recordResult(id,e.url,"skipped",QObject::tr("This item is no longer in the expected collection.")); continue;
        }
        QString destination = e.source.path;
        if (e.managed && action != "purge") {
            destination = uniqueDestination(m_root + (action == "trash" ? "/recovery" : "/originals"), e.name, true);
            if (destination.isEmpty()) { recordResult(id,e.url,"failed",QObject::tr("A safe destination name could not be allocated.")); continue; }
        }
        const QString operation = journal(action,id,e.source.path,destination,{},{{"deletedAt",QDateTime::currentMSecsSinceEpoch()}});
        if (operation.isEmpty() || !execute("UPDATE operations SET state='ready' WHERE id=?", {operation}) || !finalizeOperation(operation))
            recordResult(id,e.url,"failed",m_error);
        else recordResult(id,e.url,"done");
        if (progress) progress(double(++done)/qMax(qsizetype(1),ids.size()),QObject::tr("Updating library items…"));
    }
    return snapshot();
}
LibrarySnapshot LibraryStore::copyFiles(const QList<QUrl> &urls, const QStringList &ids,
                                        const QUrl &destination, bool importing,
                                        const std::shared_ptr<std::atomic_bool> &cancel, const Progress &progress)
{
    const QString directory = importing ? m_root + "/originals" : literalPath(destination);
    if (directory.isEmpty() || !QFileInfo(directory).isDir() || !QFileInfo(directory).isWritable()) {
        m_error = QObject::tr("The destination folder is not writable. Choose another folder."); return snapshot();
    }
    QList<QPair<QString,QUrl>> inputs;
    if (importing) for (const auto &url : urls.mid(0, MaxEntries)) inputs.append({uuid(),url});
    else for (const auto &id : ids.mid(0, MaxEntries))
        inputs.append({id,id.startsWith("session:") ? QUrl(id.mid(8)) : find(id).url});
    const QString batch = uuid();
    int done = 0;
    for (const auto &input : inputs) {
        if (cancel->load()) { recordResult(input.first,input.second,"canceled"); continue; }
        const QString src = literalPath(input.second);
        const QFileInfo info(src);
        const PhotoEntry original = importing ? PhotoEntry() : find(input.first);
        if (src.isEmpty() || !info.isFile() || !info.isReadable() || info.isSymLink()
            || (!importing && !input.first.startsWith("session:") && (original.id.isEmpty() || original.tombstone || original.deletedAt))) {
            recordResult(input.first,input.second,"failed",QObject::tr("The source is unavailable or no longer exportable.")); continue;
        }
        const QString mime = QMimeDatabase().mimeTypeForFile(info,QMimeDatabase::MatchExtension).name();
        if (importing && !mime.startsWith("image/") && !mime.startsWith("video/")) {
            recordResult(input.first,input.second,"failed",QObject::tr("This file is not a supported photo or video.")); continue;
        }
        const QString dst = uniqueDestination(directory,info.fileName(),importing);
        const QString temporary = QDir(directory).filePath(".nkphotos-"+uuid()+".partial");
        if (dst.isEmpty()) { recordResult(input.first,input.second,"failed",QObject::tr("A safe destination name could not be allocated.")); continue; }
        QVariantMap payload{{"batch",batch},{"importedAt",QDateTime::currentMSecsSinceEpoch()}};
        const QString operation = journal(importing ? "import" : "export",input.first,src,dst,temporary,payload);
        if (operation.isEmpty()) { recordResult(input.first,input.second,"failed",m_error); continue; }
        QFile source(src), target(temporary);
        QString error;
        if (!source.open(QIODevice::ReadOnly)) error = source.errorString();
        else if (!target.open(QIODevice::WriteOnly | QIODevice::NewOnly)) error = target.errorString();
        if(error.isEmpty()) {
            payload.insert("temporaryIdentity",fileIdentity(temporary));
            if(!execute("UPDATE operations SET payload=? WHERE id=?",{QString::fromUtf8(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact)),operation}))error=m_error;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 copied = 0;
        QByteArray buffer(1024*1024, Qt::Uninitialized);
        while (error.isEmpty() && !cancel->load()) {
            const qint64 bytes = source.read(buffer.data(),buffer.size());
            if (bytes < 0) { error = source.errorString(); break; }
            if (!bytes) break;
            if (target.write(buffer.constData(),bytes) != bytes) { error = target.errorString(); break; }
            hash.addData(QByteArrayView(buffer.constData(),bytes)); copied += bytes;
            if (progress) progress((done + qMin(1.0,double(copied)/qMax(qint64(1),info.size())))/qMax(qsizetype(1),inputs.size()),
                                   importing ? QObject::tr("Importing %1").arg(info.fileName()) : QObject::tr("Exporting %1").arg(info.fileName()));
        }
        if (error.isEmpty() && !cancel->load() && (!target.flush() || copied != info.size()
            || QFileInfo(src).lastModified() != info.lastModified())) error = QObject::tr("The source changed or the copy could not be flushed. Please retry.");
#ifdef Q_OS_UNIX
        if (error.isEmpty() && !cancel->load() && ::fsync(target.handle()) != 0)
            error = QObject::tr("The copy could not be saved to disk.");
#endif
        if(error.isEmpty() && !cancel->load())target.setFileTime(info.lastModified(),QFileDevice::FileModificationTime);
        target.close(); source.close();
        if (cancel->load() || !error.isEmpty()) {
            if(!payload.value("temporaryIdentity").toString().isEmpty() && fileIdentity(temporary)==payload.value("temporaryIdentity").toString())QFile::remove(temporary);
            failOperation(operation,error,cancel->load() ? "canceled" : "failed");
            recordResult(input.first,input.second,cancel->load() ? "canceled" : "failed",error);
        } else {
            payload.insert("sha256",QString::fromLatin1(hash.result().toHex()));
            if (!execute("UPDATE operations SET state='ready',payload=? WHERE id=?",
                         {QString::fromUtf8(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact)),operation})
                || !finalizeOperation(operation,cancel)) recordResult(input.first,input.second,cancel->load() ? "canceled" : "failed",m_error);
            else recordResult(input.first,QUrl::fromLocalFile(dst),"done");
        }
        ++done;
        if (progress) progress(double(done)/qMax(qsizetype(1),inputs.size()),importing ? QObject::tr("Importing originals…") : QObject::tr("Exporting originals…"));
    }
    return snapshot();
}

LibrarySnapshot LibraryStore::album(const QString &action, const QString &requestedId,
                                   const QString &name, const QStringList &ids)
{
    for(const auto &mediaId:ids)if(mediaId.startsWith("session:")) {
        m_error=QObject::tr("Import temporary files before organizing them in the library.");return snapshot();
    }
    QString id = requestedId;
    if ((action == "create" || action == "rename") && name.trimmed().isEmpty()) {
        m_error = QObject::tr("Enter an album name."); return snapshot();
    }
    if (!begin()) return snapshot();
    bool ok = true;
    if (action == "create") { id = uuid(); ok = execute("INSERT INTO albums(id,name) VALUES(?,?)", {id,name.trimmed().left(200)}); }
    else if (action == "rename") ok = execute("UPDATE albums SET name=? WHERE id=?", {name.trimmed().left(200),id});
    else if (action == "delete") ok = execute("DELETE FROM albums WHERE id=?", {id});
    else if (action == "cover") ok = execute("UPDATE albums SET cover_id=? WHERE id=? AND EXISTS(SELECT 1 FROM album_members WHERE album_id=? AND media_id=?)", {ids.value(0),id,id,ids.value(0)});
    else if (action != "add" && action != "remove") { rollback(); return snapshot(); }
    if (action == "create" || action == "add") {
        for (const auto &mediaId : ids.mid(0,MaxEntries)) {
            ok = ok && execute("INSERT OR IGNORE INTO album_members(album_id,media_id,position) "
                               "SELECT ?,id,COALESCE((SELECT MAX(position)+1 FROM album_members WHERE album_id=?),0) "
                               "FROM media WHERE id=? AND tombstone=0 AND deleted_at=0", {id,id,mediaId});
        }
    } else if (action == "remove") {
        for (const auto &mediaId : ids) ok = ok && execute("DELETE FROM album_members WHERE album_id=? AND media_id=?", {id,mediaId});
    }
    if (!ok) rollback(); else ok = commit();
    auto result = snapshot();
    if (ok && action == "create") result.createdAlbum = id;
    return result;
}
LibrarySnapshot LibraryStore::memory(const QString &action, const QString &requestedId,
                                    const QString &name, const QStringList &ids, const QString &cover,
                                    double seconds, const QUrl &music, bool muted)
{
    if (action != "create" && action != "update" && action != "delete") return snapshot();
    for(const auto &mediaId:ids)if(mediaId.startsWith("session:")) {
        m_error=QObject::tr("Import temporary files before organizing them in the library.");return snapshot();
    }
    if (action != "delete" && name.trimmed().isEmpty()) { m_error = QObject::tr("Enter a memory name."); return snapshot(); }
    if (!music.isEmpty() && (!music.isLocalFile() || !QFileInfo(music.toLocalFile()).isReadable())) {
        m_error = QObject::tr("Choose a readable local music file."); return snapshot();
    }
    QStringList eligible;
    for (const auto &mediaId : ids.mid(0,MaxEntries)) {
        const PhotoEntry e = find(mediaId);
        if (!e.id.isEmpty() && !e.hidden && !e.deletedAt && !e.tombstone && !eligible.contains(mediaId)) eligible.append(mediaId);
    }
    const QString id = action == "create" ? uuid() : requestedId;
    if(action=="update") {
        // Playback omits hidden/deleted items. Editing playback settings must not erase their
        // durable membership, so restoring/unhiding them also restores this memory association.
        QSqlQuery previous(m_db);previous.prepare("SELECT media_id FROM memory_members WHERE memory_id=? ORDER BY position");previous.addBindValue(id);previous.exec();
        int position=0;
        while(previous.next()) {
            const QString mediaId=previous.value(0).toString();const PhotoEntry e=find(mediaId);
            if(!e.id.isEmpty() && !e.tombstone && (e.hidden || e.deletedAt) && !eligible.contains(mediaId))
                eligible.insert(qMin(position,int(eligible.size())),mediaId);
            ++position;
        }
    }
    if (!begin()) return snapshot();
    bool ok = true;
    if (action == "delete") ok = execute("DELETE FROM memories WHERE id=?", {id});
    else {
        if (action == "create") ok = execute("INSERT INTO memories(id,name,created_at) VALUES(?,?,?)", {id,name.trimmed().left(200),QDateTime::currentMSecsSinceEpoch()});
        ok = ok && execute("UPDATE memories SET name=?,cover_id=?,seconds=?,music=?,muted=? WHERE id=?",
                           {name.trimmed().left(200),eligible.contains(cover) ? cover : eligible.value(0),
                            qBound(1.0,seconds,60.0),music.toString(),muted,id})
                && execute("DELETE FROM memory_members WHERE memory_id=?", {id});
        int position = 0;
        for (const auto &mediaId : eligible) ok = ok && execute("INSERT INTO memory_members(memory_id,media_id,position) VALUES(?,?,?)", {id,mediaId,position++});
    }
    if (!ok) rollback(); else ok = commit();
    auto result = snapshot();
    if (ok && action == "create") result.createdMemory = id;
    return result;
}

LibrarySnapshot LibraryStore::metadata(const QString &id, const QString &title,
                                      const QString &description, const QStringList &keywords)
{
    QStringList clean;
    for (const auto &keyword : keywords.mid(0, 100)) if (!keyword.trimmed().isEmpty()) clean.append(keyword.trimmed().left(200));
    clean.removeDuplicates();
    execute("UPDATE media SET title=?,description=?,keywords=? WHERE id=? AND tombstone=0",
            {title.left(1000),description.left(20000),json(clean),id});
    return snapshot();
}
