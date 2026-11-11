// SPDX-License-Identifier: GPL-3.0-only
#include "PhotoLibrary.h"
#include <QtConcurrentRun>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QCollator>
#include <QSet>

PhotoLibrary::PhotoLibrary(std::shared_ptr<ImageRegistry> registry, QObject *parent)
    : QAbstractListModel(parent), m_registry(std::move(registry))
{
    m_pool.setMaxThreadCount(1);
    m_folders = m_settings.value("folders").toStringList().mid(0, 64);
    m_favorites = m_settings.value("favorites").toStringList().mid(0, 20000);
    m_newestFirst = m_settings.value("newestFirst", true).toBool();
    QMimeDatabase db;
    for (const auto &mime : QImageReader::supportedMimeTypes())
        m_extensions.append(db.mimeTypeForName(QString::fromLatin1(mime)).suffixes());
    m_extensions.removeDuplicates();
    m_debounce.setInterval(350); m_debounce.setSingleShot(true);
    connect(&m_files, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_files, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_debounce, &QTimer::timeout, this, &PhotoLibrary::refresh);
    connect(&m_scan, &QFutureWatcher<ScanResult>::finished, this, [this] {
        if (m_runningGeneration == m_generation) {
            const auto result = m_scan.result();
            m_entries = result.entries;
            m_message = result.unavailable.isEmpty() ? QString() : tr("Some folders are unavailable. Reopen them with Add folder to grant access again.");
            if (result.limited) m_message = tr("Showing up to 20,000 images. Choose a smaller folder to see the remaining images.");
            if (!m_requestedSelection.isEmpty()) m_selected = m_requestedSelection;
            m_requestedSelection.clear();
            {
                QWriteLocker locker(&m_registry->lock);
                m_registry->sources.clear();
                for (const auto &entry : m_entries) m_registry->sources.insert(entry.token, entry.source);
            }
            rebuild(); updateWatches();
            if (m_openAfterScan && selectedIndex() >= 0) emit viewRequested();
            m_openAfterScan = false;
        }
        m_loading = false;
        emit changed();
        if (m_pending) startScan();
    });
    QTimer::singleShot(0, this, &PhotoLibrary::refresh);
}

PhotoLibrary::~PhotoLibrary()
{
    if (m_cancel) m_cancel->store(true);
    m_pool.waitForDone();
}

int PhotoLibrary::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_visible.size(); }
QHash<int, QByteArray> PhotoLibrary::roleNames() const
{
    return {{NameRole,"photoName"}, {UrlRole,"photoUrl"}, {ImageRole,"imageSource"},
            {FavoriteRole,"favorite"}, {ModifiedRole,"modified"}};
}
QVariant PhotoLibrary::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) return {};
    const auto &item = m_visible.at(index.row());
    switch (role) {
    case NameRole: return item.name;
    case UrlRole: return item.url;
    case ImageRole: return "image://photos/" + item.token;
    case FavoriteRole: return m_favorites.contains(item.url.toString());
    case ModifiedRole: return QDateTime::fromMSecsSinceEpoch(item.source.modified);
    default: return {};
    }
}
QVariantList PhotoLibrary::folders() const
{
    QVariantList list;
    for (const auto &value : m_folders) {
        const QUrl url(value);
        list.append(QVariantMap{{"url",url},{"name",QFileInfo(url.toLocalFile()).fileName()},{"path",url.toLocalFile()}});
    }
    return list;
}
int PhotoLibrary::selectedIndex() const
{
    for (int i = 0; i < m_visible.size(); ++i) if (m_visible.at(i).url == m_selected) return i;
    return -1;
}
QVariantMap PhotoLibrary::selected() const
{
    const int i = selectedIndex();
    if (i < 0) return {};
    const auto &item = m_visible.at(i);
    return {{"name",item.name},{"url",item.url},{"path",item.source.path},
            {"imageSource","image://photos/" + item.token},
            {"bytes",item.source.bytes},{"modified",QDateTime::fromMSecsSinceEpoch(item.source.modified)},
            {"favorite",m_favorites.contains(item.url.toString())}};
}
QStringList PhotoLibrary::fileFilters() const
{
    QStringList globs;
    for (const auto &ext : m_extensions) globs.append("*." + ext);
    return {tr("Images (%1)").arg(globs.join(' ')), tr("All files (*)")};
}
void PhotoLibrary::persist()
{
    m_settings.setValue("folders",m_folders);
    m_settings.setValue("favorites",m_favorites);
    m_settings.setValue("newestFirst",m_newestFirst);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        m_message = tr("Preferences could not be saved. Your images have not been changed.");
        emit changed();
    }
}
void PhotoLibrary::addFolder(const QUrl &url)
{
    QFileInfo info(url.toLocalFile());
    if (!url.isLocalFile() || !info.isDir() || !info.isReadable()) {
        m_message = tr("This folder is not accessible. Choose it again using the folder picker."); emit changed(); return;
    }
    const QUrl normalized = QUrl::fromLocalFile(info.absoluteFilePath());
    if (!m_folders.contains(normalized.toString())) {
        if (m_folders.size() >= 64) { m_message = tr("You can add up to 64 folders."); emit changed(); return; }
        m_folders.append(normalized.toString()); persist();
    }
    showCollection("folder", normalized);
    emit galleryRequested();
}
void PhotoLibrary::removeFolder(const QUrl &url)
{
    m_folders.removeAll(url.toString()); persist();
    if (m_folder == url) showCollection("all"); else refresh();
}
void PhotoLibrary::showCollection(const QString &mode, const QUrl &folder)
{
    if (mode != "all" && mode != "favorites" && mode != "folder" && mode != "session") return;
    if (mode == "folder" && !m_folders.contains(folder.toString())) return;
    m_mode = mode; m_folder = folder; m_search.clear(); m_selected.clear();
    m_requestedSelection.clear(); m_openAfterScan = false;
    beginResetModel(); m_entries.clear(); m_visible.clear(); endResetModel();
    scheduleScan();
}
void PhotoLibrary::openFiles(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) return;
    if (urls.size() == 1 && urls.first().isLocalFile() && QFileInfo(urls.first().toLocalFile()).isDir()) {
        addFolder(urls.first()); return;
    }
    m_session.clear();
    for (const auto &url : urls) if (url.isLocalFile() && m_session.size() < 20000) {
        const QUrl absolute = QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absoluteFilePath());
        if (!m_session.contains(absolute)) m_session.append(absolute);
    }
    if (m_session.isEmpty()) { m_message = tr("Choose local image files."); emit changed(); return; }
    showCollection("session");
    m_requestedSelection = m_session.first(); m_openAfterScan = true;
}
void PhotoLibrary::select(int index)
{
    if (index < 0 || index >= m_visible.size()) return;
    m_selected = m_visible.at(index).url; updateWatches(); emit changed();
}
void PhotoLibrary::navigate(int delta)
{
    if (m_visible.isEmpty()) return;
    select(qBound(0, selectedIndex() + delta, int(m_visible.size()) - 1));
}
void PhotoLibrary::toggleFavorite()
{
    if (selectedIndex() < 0) return;
    const QString key = m_selected.toString();
    if (m_favorites.contains(key)) m_favorites.removeAll(key);
    else if (m_favorites.size() < 20000) m_favorites.append(key);
    persist(); rebuild();
}
void PhotoLibrary::setSearch(const QString &value) { if (m_search == value) return; m_search = value; rebuild(); }
void PhotoLibrary::setNewestFirst(bool value) { if (m_newestFirst == value) return; m_newestFirst = value; persist(); rebuild(); }
void PhotoLibrary::dismissMessage() { m_message.clear(); emit changed(); }
void PhotoLibrary::refresh() { scheduleScan(); }
void PhotoLibrary::scheduleScan()
{
    ++m_generation; m_pending = true;
    if (m_cancel) m_cancel->store(true);
    if (!m_loading) startScan();
}
void PhotoLibrary::startScan()
{
    m_pending = false; m_loading = true; m_runningGeneration = m_generation;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    QStringList directories;
    QList<QUrl> files;
    if (m_mode == "folder") directories.append(m_folder.toLocalFile());
    else if (m_mode == "session") files = m_session;
    else if (m_mode == "favorites") for (const auto &url : m_favorites) files.append(QUrl(url));
    else for (const auto &url : m_folders) directories.append(QUrl(url).toLocalFile());
    m_scan.setFuture(QtConcurrent::run(&m_pool, &PhotoLibrary::scan, directories, files, m_extensions, m_cancel));
    emit changed();
}
ScanResult PhotoLibrary::scan(const QStringList &directories, const QList<QUrl> &files,
                             const QStringList &extensions, const std::shared_ptr<std::atomic_bool> &cancel)
{
    ScanResult result;
    QSet<QString> known;
    auto append = [&](const QFileInfo &info, bool explicitFile) {
        if (cancel->load() || !info.isFile() || info.isSymLink() || !info.isReadable()) return;
        if (!explicitFile && !extensions.contains(info.suffix().toLower())) return;
        const QString path = info.absoluteFilePath();
        if (known.contains(path)) return;
        if (result.entries.size() >= 20000) { result.limited = true; return; }
        known.insert(path);
        ImageSource source{path,info.size(),info.lastModified().toMSecsSinceEpoch()};
        result.entries.append({QUrl::fromLocalFile(path), info.fileName(), source, ImageProvider::token(source)});
    };
    for (const auto &url : files) { if (cancel->load()) break; append(QFileInfo(url.toLocalFile()), true); }
    for (const auto &path : directories) {
        if (cancel->load() || result.limited) break;
        const QFileInfo directory(path);
        if (!directory.isDir() || !directory.isReadable()) { result.unavailable.append(path); continue; }
        QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::NoIteratorFlags);
        while (it.hasNext() && !cancel->load() && !result.limited) { it.next(); append(it.fileInfo(), false); }
    }
    return result;
}
void PhotoLibrary::rebuild()
{
    beginResetModel();
    m_visible.clear();
    for (const auto &item : m_entries) {
        if (!item.name.contains(m_search, Qt::CaseInsensitive)) continue;
        if (m_mode == "favorites" && !m_favorites.contains(item.url.toString())) continue;
        m_visible.append(item);
    }
    QCollator collator; collator.setNumericMode(true); collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_visible.begin(), m_visible.end(), [&](const auto &a, const auto &b) {
        if (m_newestFirst && a.source.modified != b.source.modified) return a.source.modified > b.source.modified;
        const int order = collator.compare(a.name,b.name);
        return order == 0 ? a.source.path < b.source.path : order < 0;
    });
    endResetModel(); emit changed();
}
void PhotoLibrary::updateWatches()
{
    if (!m_files.directories().isEmpty()) m_files.removePaths(m_files.directories());
    if (!m_files.files().isEmpty()) m_files.removePaths(m_files.files());
    QStringList paths;
    if (m_mode == "folder") paths.append(m_folder.toLocalFile());
    else if (m_mode == "all") for (const auto &url : m_folders) paths.append(QUrl(url).toLocalFile());
    if (selectedIndex() >= 0) paths.append(m_selected.toLocalFile());
    if (!paths.isEmpty()) m_files.addPaths(paths);
}
