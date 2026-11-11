// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtCore
import Kos.Ui

KosAppWindow {
    id: root
    objectName: "photosWindow"
    visible: true
    width: preferences.windowWidth; height: preferences.windowHeight
    minimumWidth: 740; minimumHeight: 480
    title: viewing && currentPhoto.name ? currentPhoto.name + " — NkPhotos" : "NkPhotos"
    // Presentation-only state is addressable by the native screenshot option.
    property bool viewing: false
    property bool infoVisible: false
    property string browserPage: "library"
    property alias selectionMode: gallery.selectionMode
    property real savedGridY: 0
    property string savedPhoto: ""
    property var scrollPositions: ({})
    property var playbackItems: []
    property var playbackMemory: ({})
    property int playbackIndex: 0
    property bool memoryPlaying: false
    readonly property var currentPhoto: memoryPlaying ? playbackItems[playbackIndex] || ({}) : photoLibrary.selected
    readonly property string viewKey: browserPage + ":" + photoLibrary.mode + ":" + photoLibrary.collectionId + ":" + photoLibrary.folder.toString()
    readonly property bool popupOpen: infoPopup.visible || addMenu.visible || viewMenu.visible || moreMenu.visible || appearanceMenu.visible || folderMenu.visible || collectionMenu.visible || actionMenu.visible || dialogs.anyOpen || memoryEditor.visible || memoryEditor.pickerOpen
    readonly property string collectionTitle: browserPage === "albums" ? qsTr("Albums") : browserPage === "memories" ? qsTr("Memories")
        : photoLibrary.mode === "favorites" ? qsTr("Favorites") : photoLibrary.mode === "recent" ? qsTr("Recent imports")
        : photoLibrary.mode === "hidden" ? qsTr("Hidden") : photoLibrary.mode === "trash" ? qsTr("Recently deleted")
        : photoLibrary.mode === "session" ? qsTr("Opened files") : photoLibrary.mode === "folder" ? folderTitle()
        : photoLibrary.mode === "album" || photoLibrary.mode === "memory" ? currentCollection().name || qsTr("Collection")
        : photoLibrary.mediaFilter === "video" ? qsTr("Videos") : qsTr("All photos")
    function folderTitle() {
        for (const folder of photoLibrary.folders) if (folder.url.toString() === photoLibrary.folder.toString()) return folder.name || folder.path
        return qsTr("Folder")
    }
    function currentCollection() {
        const list = photoLibrary.mode === "memory" ? photoLibrary.memories : photoLibrary.albums
        for (const collection of list) if (collection.id === photoLibrary.collectionId) return collection
        return ({})
    }
    function rememberScroll() { const positions = Object.assign({}, scrollPositions); positions[viewKey] = gallery.grid.contentY; scrollPositions = positions }
    function restoreScroll() { Qt.callLater(function() { gallery.grid.contentY = Math.max(0, Math.min(scrollPositions[viewKey] || 0, Math.max(0,gallery.grid.contentHeight-gallery.grid.height))) }) }
    function leaveViewer() { infoVisible = false; viewer.stopSlideshow(); memoryPlaying = false; viewing = false }
    function showCollection(mode, folder) {
        rememberScroll(); leaveViewer(); browserPage = "library"; selectionMode = false
        photoLibrary.showCollection(mode === "videos" ? "all" : mode, folder || "")
        photoLibrary.mediaFilter = mode === "videos" ? "video" : "all"; photoLibrary.dateFilter = ""; photoLibrary.dateView = "all"
        restoreScroll()
    }
    function browse(page) { rememberScroll(); leaveViewer(); photoLibrary.clearSelection(); browserPage = page; selectionMode = false }
    function showAlbum(id) { rememberScroll(); leaveViewer(); browserPage = "library"; photoLibrary.showAlbum(id); photoLibrary.dateView = "all"; photoLibrary.dateFilter = ""; photoLibrary.mediaFilter = "all"; restoreScroll() }
    function showMemory(id) { rememberScroll(); leaveViewer(); browserPage = "library"; photoLibrary.showMemory(id); photoLibrary.dateView = "all"; photoLibrary.dateFilter = ""; photoLibrary.mediaFilter = "all"; restoreScroll() }
    function openSelected() {
        if (photoLibrary.selectedIndex < 0) return
        if (!viewing) { savedGridY = gallery.grid.contentY; savedPhoto = photoLibrary.selected.mediaId || photoLibrary.selected.id || "" }
        memoryPlaying = false; viewing = true; viewer.forceActiveFocus()
    }
    function returnToGallery() {
        const wasMemory = memoryPlaying; const wasViewing = viewing
        leaveViewer()
        Qt.callLater(function() {
            if (wasViewing && !wasMemory) {
                gallery.grid.contentY = Math.max(0, Math.min(savedGridY, Math.max(0,gallery.grid.contentHeight-gallery.grid.height)))
                const id = photoLibrary.selected.mediaId || photoLibrary.selected.id || ""
                if (photoLibrary.selectedIndex >= 0 && id !== savedPhoto) gallery.grid.positionViewAtIndex(photoLibrary.selectedIndex, GridView.Contain)
            }
            gallery.grid.forceActiveFocus()
        })
    }
    function startMemory(memory) {
        const items = Array.from(photoLibrary.memoryItems(memory.id))
        if (!items.length) return
        savedGridY = gallery.grid.contentY; savedPhoto = photoLibrary.selected.mediaId || photoLibrary.selected.id || ""
        playbackItems = items; playbackMemory = Object.assign({},memory); playbackIndex = 0
        memoryPlaying = true; viewing = true; infoVisible = false
        viewer.slideshowEnabled = true; viewer.intervalSeconds = memory.seconds || 5; viewer.musicUrl = memory.music || ""; viewer.musicMuted = !!memory.muted
        viewer.playing = true; viewer.forceActiveFocus()
    }
    function navigate(delta) {
        if (memoryPlaying) playbackIndex = Math.max(0,Math.min(playbackItems.length-1,playbackIndex+delta))
        else photoLibrary.navigate(delta)
    }
    function frozenActionIds() {
        if (viewing) { const id = currentPhoto.mediaId || currentPhoto.id; return id ? [id] : [] }
        return Array.from(photoLibrary.actionIds())
    }
    function refreshPlaybackInformation() {
        if (!memoryPlaying || !playbackItems[playbackIndex]) return
        const snapshot = playbackItems[playbackIndex]
        const latest = photoLibrary.entry(snapshot.mediaId || snapshot.id)
        if (!latest.id) return
        const metadata = {title:latest.title, description:latest.description, keywords:Array.from(latest.keywords || []), favorite:latest.favorite, hidden:latest.hidden, trashed:latest.trashed}
        const previous = {title:snapshot.title, description:snapshot.description, keywords:Array.from(snapshot.keywords || []), favorite:snapshot.favorite, hidden:snapshot.hidden, trashed:snapshot.trashed}
        if (JSON.stringify(metadata) === JSON.stringify(previous)) return
        const items = playbackItems.slice()
        // Refresh library details without changing the fixed playback order or media sources.
        items[playbackIndex] = Object.assign({},snapshot,metadata)
        playbackItems = items
    }
    onPlaybackIndexChanged: refreshPlaybackInformation()
    function showActions(anchor) { actionMenu.show(anchor, frozenActionIds()) }
    function showCollectionActions(collection, anchor, memory) { collectionMenu.collection = Object.assign({},collection); collectionMenu.memory = memory; collectionMenu.anchorItem = anchor; collectionMenu.openAtAnchor() }
    function showInformation(id) { if (!memoryPlaying) photoLibrary.selectById(id); infoVisible = true }
    function toggleCurrentFavorite() {
        const value = !currentPhoto.favorite
        photoLibrary.setFavorite(frozenActionIds(),value)
        if (memoryPlaying) { const items = playbackItems.slice(); items[playbackIndex] = Object.assign({},items[playbackIndex],{favorite:value}); playbackItems = items }
    }
    Settings {
        id: preferences; objectName: "preferences"; category: "Window"
        property int windowWidth: 1080
        property int windowHeight: 720
        property string appearance: "system"
        property bool sidebar: true
        property int tileSize: 170
        property bool showNames: false
        property bool cropThumbnails: true
    }
    Binding { target: AppTheme; property: "appearanceMode"; value: preferences.appearance }
    Binding { target: AppTheme; property: "compactDesign"; value: true }
    onClosing: if (visibility === Window.Windowed) { preferences.windowWidth = width; preferences.windowHeight = height }
    onActiveChanged: if (active && !memoryEditor.pickerOpen) photoLibrary.refresh()
    onInfoVisibleChanged: { if (infoVisible) infoPopup.open(); else infoPopup.close() }
    onPopupOpenChanged: if (popupOpen && memoryPlaying) viewer.playing = false
    Connections {
        target: photoLibrary
        function onChanged() { root.refreshPlaybackInformation() }
        function onViewRequested() { root.browserPage = "library"; root.openSelected() }
        function onGalleryRequested() { root.browserPage = "library"; root.returnToGallery() }
        function onAlbumCreated(id) { root.showAlbum(id) }
        function onMemoryCreated(id) {
            root.showMemory(id)
            Qt.callLater(function() { const memory = root.currentCollection(); if (memory.id) memoryEditor.edit(memory) })
        }
    }
    Shortcut { sequences: [StandardKey.Open]; enabled: !root.popupOpen; onActivated: dialogs.openFiles() }
    Shortcut { sequence: "Ctrl+Shift+O"; enabled: !root.popupOpen; onActivated: dialogs.addFolder() }
    Shortcut { sequence: "Ctrl+Shift+I"; enabled: !root.popupOpen; onActivated: dialogs.importFiles() }
    Shortcut { sequence: "Ctrl+B"; enabled: !root.viewing && !root.popupOpen; onActivated: preferences.sidebar = !preferences.sidebar }
    Shortcut { sequence: "F5"; enabled: !root.popupOpen; onActivated: photoLibrary.refresh() }
    Shortcut { sequence: "F11"; onActivated: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen() }
    Shortcut { sequence: "Escape"; enabled: !root.popupOpen; onActivated: { if (root.viewing) root.returnToGallery(); else { photoLibrary.clearSelection(); root.selectionMode = false } } }
    Shortcut { sequence: "Left"; enabled: root.viewing && !root.popupOpen; onActivated: root.navigate(-1) }
    Shortcut { sequence: "Right"; enabled: root.viewing && !root.popupOpen; onActivated: root.navigate(1) }
    Shortcut { sequence: "Ctrl+I"; enabled: !!root.currentPhoto.name && !dialogs.anyOpen; onActivated: root.infoVisible = !root.infoVisible }
    Shortcut { sequences: [StandardKey.Find]; enabled: !root.viewing && !root.popupOpen; onActivated: search.forceActiveFocus() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: !root.viewing && root.browserPage === "library" && !root.popupOpen && !search.activeFocus; onActivated: { photoLibrary.dateView = "all"; photoLibrary.selectAll() } }
    Shortcut { sequence: "Delete"; enabled: !root.popupOpen && !search.activeFocus && (root.viewing || root.browserPage === "library") && root.frozenActionIds().length > 0 && !root.frozenActionIds().some(id => photoLibrary.entry(id).temporary); onActivated: dialogs.confirm((root.viewing ? !!root.currentPhoto.trashed : photoLibrary.mode === "trash") ? "permanent" : "trash",root.frozenActionIds()) }

    header: Item {
        height: LiquidStyle.toolbarButtonHeight + 28
        Rectangle { anchors.fill: parent; color: AppTheme.contentBackground }
        MouseArea {
            anchors.fill: parent; property point origin
            onPressed: mouse => { origin = Qt.point(mouse.x,mouse.y) }
            onPositionChanged: mouse => { if (pressed && (Math.abs(mouse.x-origin.x)>5 || Math.abs(mouse.y-origin.y)>5)) root.startSystemMove() }
            onDoubleClicked: root.toggleMaximized()
        }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 12; spacing: 10
            PhotoAction { objectName: "navigationButton"; label: root.viewing ? qsTr("Back to photos") : qsTr("Toggle sidebar"); iconName: root.viewing ? "chevron_left" : "sidebar_left"; onClicked: root.viewing ? root.returnToGallery() : preferences.sidebar = !preferences.sidebar }
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 60; spacing: 2
                Label { Layout.fillWidth: true; elide: Text.ElideMiddle; textFormat: Text.PlainText; text: root.viewing ? (root.memoryPlaying ? root.playbackMemory.name : root.currentPhoto.title || root.currentPhoto.name || qsTr("Item unavailable")) : root.collectionTitle; color: AppTheme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Label { visible: root.viewing; text: qsTr("%1 of %2").arg(root.memoryPlaying ? root.playbackIndex+1 : photoLibrary.selectedIndex+1).arg(root.memoryPlaying ? root.playbackItems.length : photoLibrary.count); color: AppTheme.mutedText; font.pixelSize: 11 }
            }
            LiquidTextField {
                id: search; objectName: "filenameSearch"; visible: !root.viewing && root.browserPage === "library"
                Layout.preferredWidth: root.width < 850 ? 142 : 200
                materialForm: true; placeholderText: qsTr("Search photos"); text: photoLibrary.search; onTextEdited: photoLibrary.search = text
                Accessible.name: qsTr("Search names, descriptions, keywords and dates")
                onAccepted: { if (photoLibrary.selectedIndex < 0) photoLibrary.select(0); root.openSelected() }
                Keys.onEscapePressed: { photoLibrary.search = ""; gallery.grid.forceActiveFocus() }
            }
            RowLayout {
                spacing: 4
                PhotoAction { id: addButton; objectName: "addButton"; visible: !root.viewing; label: qsTr("Add photos"); iconName: "plus"; onClicked: addMenu.openAtAnchor() }
                PhotoAction { id: viewButton; objectName: "viewOptionsButton"; visible: !root.viewing && root.browserPage === "library"; label: qsTr("View options"); iconName: "slider_horizontal_3"; onClicked: viewMenu.openAtAnchor() }
                PhotoAction { objectName: "favoriteButton"; visible: root.viewing; label: root.currentPhoto.favorite ? qsTr("Unfavorite") : qsTr("Favorite"); iconName: "heart"; iconFilled: !!root.currentPhoto.favorite; enabled: !!root.currentPhoto.name && !root.currentPhoto.trashed && !root.currentPhoto.temporary; onClicked: root.toggleCurrentFavorite() }
                PhotoAction { id: infoButton; objectName: "infoButton"; visible: root.viewing; label: qsTr("Information"); iconName: "info_circle"; iconFilled: infoPopup.visible; onClicked: root.infoVisible = !root.infoVisible }
                LiquidActionButton { objectName: "editButton"; visible: root.viewing && root.currentPhoto.mediaType === "photo"; enabled: root.currentPhoto.available && !root.currentPhoto.trashed && !editorBridge.busy; toolbarAction: true; materialForm: true; text: editorBridge.busy ? qsTr("Preparing…") : qsTr("Edit…"); Accessible.name: qsTr("Edit a copy in NkShot"); onClicked: dialogs.editItem(root.currentPhoto) }
                PhotoAction { id: itemActionsButton; objectName: "itemActionsButton"; visible: root.viewing; label: qsTr("Photo actions"); iconName: "square_arrow_up"; onClicked: root.showActions(itemActionsButton) }
                PhotoAction { id: moreButton; objectName: "moreButton"; label: qsTr("More actions"); iconName: "ellipsis"; onClicked: moreMenu.openAtAnchor() }
            }
            Rectangle { width: 1; height: 18; color: AppTheme.withAlpha(AppTheme.text,.10) }
            KosWindowControls { targetWindow: root; actions: root.windowActions; maximizeMode: true; circleSize: 14; buttonWidth: 22; buttonHeight: 22; spacing: 6 }
        }
    }
    Item {
        id: scene; anchors.fill: parent
        ColumnLayout {
            anchors.fill: parent; spacing: 0
            Rectangle {
                Layout.fillWidth: true; implicitHeight: Math.max(40,notice.implicitHeight+20)
                visible: !!photoLibrary.message || !!editorBridge.message || !!shareBridge.message
                color: AppTheme.contentSecondaryBackground
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 12
                    Label { id: notice; Layout.fillWidth: true; text: editorBridge.message || shareBridge.message || photoLibrary.message; color: AppTheme.text; wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pixelSize: 12 }
                    PhotoAction { label: qsTr("Dismiss"); iconName: "xmark"; onClicked: { photoLibrary.dismissMessage(); editorBridge.dismiss(); shareBridge.dismiss() } }
                }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 42; visible: photoLibrary.busy; color: AppTheme.contentSecondaryBackground
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 12; spacing: 12
                    Label { Layout.fillWidth: true; text: photoLibrary.operationText; color: AppTheme.text; font.pixelSize: 12; elide: Text.ElideRight }
                    ProgressBar { Layout.preferredWidth: 140; value: photoLibrary.progress; indeterminate: photoLibrary.progress < 0 }
                    LiquidActionButton { text: qsTr("Cancel"); toolbarAction: true; materialForm: true; flat: true; onClicked: photoLibrary.cancelOperation() }
                }
            }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                LibrarySidebar {
                    objectName: "librarySidebar"; Layout.preferredWidth: root.width < 850 ? 174 : 194; Layout.fillHeight: true
                    visible: preferences.sidebar && !root.viewing; browserPage: root.browserPage
                    onNavigate: (mode, folder) => root.showCollection(mode,folder)
                    onBrowse: page => root.browse(page)
                    onAddFolder: dialogs.addFolder()
                    onFolderActions: (folder,anchor) => { folderMenu.folder = Object.assign({},folder); folderMenu.anchorItem = anchor; folderMenu.openAtAnchor() }
                }
                GalleryView {
                    id: gallery; objectName: "galleryView"; Layout.fillWidth: true; Layout.fillHeight: true; visible: !root.viewing && root.browserPage === "library"
                    tileSize: preferences.tileSize; showNames: preferences.showNames; cropThumbnails: preferences.cropThumbnails
                    onOpenSelected: root.openSelected()
                    onAddFolder: dialogs.addFolder()
                    onOpenFiles: dialogs.openFiles()
                    onImportFiles: dialogs.importFiles()
                    onActionsRequested: anchor => root.showActions(anchor)
                    onCreateDateMemory: (key,label) => dialogs.createDateMemory(key,label)
                    onCollectionActions: anchor => root.showCollectionActions(root.currentCollection(),anchor,photoLibrary.mode === "memory")
                    onPlayMemory: root.startMemory(root.currentCollection())
                }
                CollectionBrowser {
                    objectName: "collectionBrowser"; Layout.fillWidth: true; Layout.fillHeight: true; visible: !root.viewing && root.browserPage !== "library"
                    memories: root.browserPage === "memories"
                    onOpenCollection: collection => memories ? root.showMemory(collection.id) : root.showAlbum(collection.id)
                    onCollectionActions: (collection,anchor) => root.showCollectionActions(collection,anchor,memories)
                    onCreateCollection: memories ? dialogs.createMemory([]) : dialogs.createAlbum([])
                    onPlayMemory: memory => root.startMemory(memory)
                }
                PhotoViewer {
                    id: viewer; objectName: "photoViewer"; Layout.fillWidth: true; Layout.fillHeight: true; visible: root.viewing
                    photo: root.currentPhoto; position: root.memoryPlaying ? root.playbackIndex : photoLibrary.selectedIndex; total: root.memoryPlaying ? root.playbackItems.length : photoLibrary.count
                    slideshowEnabled: root.memoryPlaying; intervalSeconds: root.playbackMemory.seconds || 5; musicUrl: root.playbackMemory.music || ""; musicMuted: !!root.playbackMemory.muted
                    onPrevious: root.navigate(-1)
                    onNext: root.navigate(1)
                    onAdvance: { if (root.playbackIndex + 1 < root.playbackItems.length) root.playbackIndex++; else if (loop) root.playbackIndex = 0 }
                    onFinished: root.returnToGallery()
                }
            }
        }
        DropArea { anchors.fill: parent; onDropped: drop => { if (drop.hasUrls) { photoLibrary.openFiles(drop.urls); drop.acceptProposedAction() } } }
    }
    LibraryDialogs { id: dialogs; objectName: "libraryDialogs" }
    MemoryEditor { id: memoryEditor }
    MediaInfoPanel {
        id: infoPopup; objectName: "infoPopup"; anchorItem: root.viewing ? infoButton : moreButton; sourceItem: scene; photo: root.currentPhoto
        onClosed: { root.infoVisible = false; if (root.viewing) viewer.forceActiveFocus() }
    }
    LibraryActionMenu { id: actionMenu; objectName: "selectionMenu"; sourceItem: scene; dialogs: dialogs; height: Math.min(implicitHeight, root.contentItem.height - 16); onShowInformation: mediaId => root.showInformation(mediaId); onEditPhoto: photo => dialogs.editItem(photo) }
    LiquidPopover {
        id: addMenu; objectName: "addMenu"; anchorItem: addButton; sourceItem: scene
        LiquidPopoverItem { text: qsTr("Import copies…"); iconName: "square_arrow_down"; enabled: !photoLibrary.busy; onTriggered: dialogs.importFiles() }
        LiquidPopoverItem { text: qsTr("Add referenced folder…"); iconName: "folder_badge_plus"; onTriggered: dialogs.addFolder() }
        LiquidPopoverItem { text: qsTr("Open files temporarily…"); iconName: "photo"; onTriggered: dialogs.openFiles() }
        MenuSeparator {}
        LiquidPopoverItem { text: qsTr("New album…"); iconName: "rectangle_stack"; enabled: !Array.from(photoLibrary.selectedIds).some(id => photoLibrary.entry(id).temporary); onTriggered: dialogs.createAlbum(Array.from(photoLibrary.selectedIds)) }
        LiquidPopoverItem { text: qsTr("New memory…"); iconName: "play_circle"; enabled: !Array.from(photoLibrary.selectedIds).some(id => photoLibrary.entry(id).temporary); onTriggered: dialogs.createMemory(Array.from(photoLibrary.selectedIds)) }
    }
    LiquidPopover {
        id: viewMenu; objectName: "viewMenu"; anchorItem: viewButton; sourceItem: scene; height: Math.min(implicitHeight, root.contentItem.height-16)
        LiquidPopoverItem { text: qsTr("All media"); iconName: photoLibrary.mediaFilter === "all" ? "checkmark" : ""; onTriggered: photoLibrary.mediaFilter = "all" }
        LiquidPopoverItem { text: qsTr("Photos only"); iconName: photoLibrary.mediaFilter === "photo" ? "checkmark" : ""; onTriggered: photoLibrary.mediaFilter = "photo" }
        LiquidPopoverItem { text: qsTr("Videos only"); iconName: photoLibrary.mediaFilter === "video" ? "checkmark" : ""; onTriggered: photoLibrary.mediaFilter = "video" }
        MenuSeparator {}
        LiquidPopoverItem { text: qsTr("Newest first"); iconName: photoLibrary.newestFirst ? "checkmark" : ""; onTriggered: photoLibrary.newestFirst = true }
        LiquidPopoverItem { text: qsTr("By name"); iconName: !photoLibrary.newestFirst ? "checkmark" : ""; onTriggered: photoLibrary.newestFirst = false }
        MenuSeparator {}
        LiquidPopoverItem { text: qsTr("Square thumbnails"); iconName: preferences.cropThumbnails ? "checkmark" : ""; onTriggered: preferences.cropThumbnails = true }
        LiquidPopoverItem { text: qsTr("Show entire images"); iconName: !preferences.cropThumbnails ? "checkmark" : ""; onTriggered: preferences.cropThumbnails = false }
        LiquidPopoverItem { text: preferences.showNames ? qsTr("Hide filenames") : qsTr("Show filenames"); onTriggered: preferences.showNames = !preferences.showNames }
        LiquidPopoverItem { text: qsTr("Larger thumbnails"); iconName: "plus"; enabled: preferences.tileSize < 250; onTriggered: preferences.tileSize += 40 }
        LiquidPopoverItem { text: qsTr("Smaller thumbnails"); iconName: "minus"; enabled: preferences.tileSize > 130; onTriggered: preferences.tileSize -= 40 }
    }
    LiquidPopover {
        id: moreMenu; objectName: "moreMenu"; anchorItem: moreButton; sourceItem: scene
        LiquidPopoverItem { text: qsTr("Selected item actions…"); enabled: root.frozenActionIds().length > 0; onTriggered: Qt.callLater(function() { root.showActions(moreButton) }) }
        LiquidPopoverItem { text: qsTr("Select all"); visible: !root.viewing && root.browserPage === "library"; enabled: photoLibrary.count > 0; onTriggered: { photoLibrary.dateView = "all"; photoLibrary.selectAll() } }
        LiquidPopoverItem { text: qsTr("Clear selection"); visible: !root.viewing && photoLibrary.selectionCount > 0; onTriggered: photoLibrary.clearSelection() }
        MenuSeparator {}
        LiquidPopoverItem { text: qsTr("Operation results…"); enabled: photoLibrary.operationResults.length > 0; onTriggered: dialogs.showResults() }
        LiquidPopoverItem { text: qsTr("Refresh"); iconName: "arrow_clockwise"; onTriggered: photoLibrary.refresh() }
        LiquidPopoverItem { text: root.visibility === Window.FullScreen ? qsTr("Leave fullscreen") : qsTr("Fullscreen"); onTriggered: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen() }
        LiquidPopoverItem { text: qsTr("Appearance…"); iconName: "circle_lefthalf_fill"; onTriggered: Qt.callLater(function() { appearanceMenu.openAtAnchor() }) }
    }
    LiquidPopover {
        id: appearanceMenu; anchorItem: moreButton; sourceItem: scene
        LiquidPopoverItem { text: qsTr("System"); iconName: preferences.appearance === "system" ? "checkmark" : ""; onTriggered: preferences.appearance = "system" }
        LiquidPopoverItem { text: qsTr("Light"); iconName: preferences.appearance === "light" ? "checkmark" : ""; onTriggered: preferences.appearance = "light" }
        LiquidPopoverItem { text: qsTr("Dark"); iconName: preferences.appearance === "dark" ? "checkmark" : ""; onTriggered: preferences.appearance = "dark" }
    }
    LiquidPopover {
        id: folderMenu; sourceItem: scene; property var folder: ({})
        LiquidPopoverItem { text: qsTr("Locate folder…"); iconName: "folder"; onTriggered: dialogs.relinkFolder(folderMenu.folder.url) }
        LiquidPopoverItem { text: qsTr("Remove folder reference…"); iconName: "folder_badge_minus"; onTriggered: dialogs.confirm("removeFolder",[],folderMenu.folder) }
    }
    LiquidPopover {
        id: collectionMenu; objectName: "collectionMenu"; sourceItem: scene
        property var collection: ({})
        property bool memory: false
        LiquidPopoverItem { text: qsTr("Play memory"); iconName: "play_fill"; visible: collectionMenu.memory; enabled: collectionMenu.collection.count > 0; onTriggered: root.startMemory(collectionMenu.collection) }
        LiquidPopoverItem { text: qsTr("Edit memory…"); visible: collectionMenu.memory; onTriggered: memoryEditor.edit(collectionMenu.collection) }
        LiquidPopoverItem { text: qsTr("Rename album…"); visible: !collectionMenu.memory; onTriggered: dialogs.renameAlbum(collectionMenu.collection) }
        LiquidPopoverItem {
            text: qsTr("Create memory from album…"); visible: !collectionMenu.memory; enabled: collectionMenu.collection.count > 0
            onTriggered: { root.showAlbum(collectionMenu.collection.id); photoLibrary.selectAll(); const ids = Array.from(photoLibrary.actionIds()); photoLibrary.clearSelection(); dialogs.createMemory(ids) }
        }
        LiquidPopoverItem { text: collectionMenu.memory ? qsTr("Delete memory…") : qsTr("Delete album…"); iconName: "trash"; onTriggered: dialogs.confirm(collectionMenu.memory ? "deleteMemory" : "deleteAlbum",[],collectionMenu.collection) }
    }
}
