// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import QtCore
import Kos.Ui

KosAppWindow {
    id: root
    visible: true
    width: preferences.windowWidth
    height: preferences.windowHeight
    minimumWidth: 740
    minimumHeight: 480
    title: viewing && photoLibrary.selected.name ? photoLibrary.selected.name + " — NkPhotos" : "NkPhotos"
    property bool viewing: false
    property bool infoVisible: false
    property url editSource
    readonly property string collectionTitle: photoLibrary.mode === "favorites" ? qsTr("Favorites")
        : photoLibrary.mode === "session" ? qsTr("Opened images")
        : photoLibrary.mode === "folder" ? folderTitle() : qsTr("All photos")
    function folderTitle() {
        const folders = photoLibrary.folders
        for (let i = 0; i < folders.length; ++i)
            if (folders[i].url.toString() === photoLibrary.folder.toString()) return folders[i].name || folders[i].path
        return qsTr("All photos")
    }
    Settings {
        id: preferences
        objectName: "preferences"
        category: "Window"
        property int windowWidth: 1080
        property int windowHeight: 720
        property string appearance: "system"
        property bool sidebar: true
    }
    Binding { target: AppTheme; property: "appearanceMode"; value: preferences.appearance }
    Binding { target: AppTheme; property: "compactDesign"; value: true }
    onClosing: {
        if (visibility === Window.Windowed) { preferences.windowWidth = width; preferences.windowHeight = height }
    }
    onActiveChanged: if (active) photoLibrary.refresh()
    Connections {
        target: photoLibrary
        function onViewRequested() { root.viewing = true; viewer.forceActiveFocus() }
    }
    function showCollection(mode, folder) {
        viewing = false
        photoLibrary.showCollection(mode, folder || "")
    }
    function openSelected() {
        if (photoLibrary.selectedIndex < 0) return
        viewing = true
        viewer.forceActiveFocus()
    }
    Shortcut { sequences: [StandardKey.Open]; onActivated: filesDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+O"; onActivated: folderDialog.open() }
    Shortcut { sequence: "Ctrl+B"; onActivated: preferences.sidebar = !preferences.sidebar }
    Shortcut { sequence: "F5"; onActivated: photoLibrary.refresh() }
    Shortcut { sequence: "F11"; onActivated: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen() }
    Shortcut { sequence: "Escape"; enabled: root.viewing; onActivated: root.viewing = false }
    Shortcut { sequences: [StandardKey.Find]; enabled: !root.viewing; onActivated: search.forceActiveFocus() }
    FileDialog {
        id: filesDialog
        title: qsTr("Open images")
        fileMode: FileDialog.OpenFiles
        nameFilters: photoLibrary.fileFilters
        onAccepted: photoLibrary.openFiles(selectedFiles)
    }
    FolderDialog {
        id: folderDialog
        title: qsTr("Add a photo folder")
        onAccepted: { root.viewing = false; photoLibrary.addFolder(selectedFolder) }
    }
    FolderDialog {
        id: editFolderDialog
        title: qsTr("Choose where to save the editing copy")
        onAccepted: editorBridge.editCopy(root.editSource, selectedFolder)
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 52
            color: AppTheme.contentBackground
            RowLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 8
                KosSidebarToggle {
                    onClicked: preferences.sidebar = !preferences.sidebar
                }
                LiquidActionButton {
                    text: qsTr("Back"); toolbarAction: true; materialForm: true
                    visible: root.viewing
                    onClicked: { root.viewing = false; grid.forceActiveFocus() }
                }
                LiquidActionButton {
                    text: qsTr("Open…"); toolbarAction: true; materialForm: true
                    onClicked: filesDialog.open()
                }
                LiquidActionButton {
                    text: qsTr("Add folder…"); toolbarAction: true; materialForm: true
                    visible: !root.viewing
                    onClicked: folderDialog.open()
                }
                Item { Layout.fillWidth: true }
                LiquidTextField {
                    id: search
                    visible: !root.viewing
                    Layout.preferredWidth: Math.min(240, root.width * .23)
                    placeholderText: qsTr("Search filenames")
                    materialForm: true
                    text: photoLibrary.search
                    onTextEdited: photoLibrary.search = text
                    Accessible.name: placeholderText
                }
                LiquidActionButton {
                    toolbarAction: true; materialForm: true
                    text: photoLibrary.selected.favorite ? qsTr("Unfavorite") : qsTr("Favorite")
                    visible: root.viewing; enabled: photoLibrary.selectedIndex >= 0
                    onClicked: photoLibrary.toggleFavorite()
                }
                LiquidActionButton {
                    toolbarAction: true; materialForm: true
                    text: editorBridge.busy ? qsTr("Preparing…") : qsTr("Edit in NkShot…")
                    visible: root.viewing; enabled: photoLibrary.selectedIndex >= 0 && !editorBridge.busy
                    onClicked: { root.editSource = photoLibrary.selected.url; editFolderDialog.open() }
                }
                LiquidActionButton {
                    text: qsTr("Info"); toolbarAction: true; materialForm: true
                    visible: root.viewing; checkable: true; checked: root.infoVisible
                    onClicked: root.infoVisible = !root.infoVisible
                }
                LiquidActionButton {
                    text: preferences.appearance === "system" ? qsTr("System") : preferences.appearance === "light" ? qsTr("Light") : qsTr("Dark")
                    toolbarAction: true; materialForm: true
                    onClicked: preferences.appearance = preferences.appearance === "system" ? "light" : preferences.appearance === "light" ? "dark" : "system"
                    Accessible.name: qsTr("Change appearance")
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: notice.implicitHeight + 20
            visible: !!photoLibrary.message || !!editorBridge.message
            color: AppTheme.contentSecondaryBackground
            RowLayout {
                anchors.fill: parent; anchors.margins: 10
                Label {
                    id: notice; Layout.fillWidth: true
                    text: editorBridge.message || photoLibrary.message
                    color: AppTheme.text; wrapMode: Text.Wrap; textFormat: Text.PlainText
                }
                LiquidActionButton {
                    text: qsTr("Dismiss"); materialForm: true
                    onClicked: { photoLibrary.dismissMessage(); editorBridge.dismiss() }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 196; Layout.fillHeight: true
                visible: preferences.sidebar && (!root.viewing || root.width >= 1000)
                color: AppTheme.contentSecondaryBackground
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 6
                    Label { text: qsTr("LIBRARY"); color: AppTheme.mutedText; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.topMargin: 10; Layout.bottomMargin: 8 }
                    Repeater {
                        model: [{key: "all", title: qsTr("All photos")}, {key: "favorites", title: qsTr("Favorites")}, {key: "session", title: qsTr("Opened images")}]
                        delegate: KosItemDelegate {
                            required property var modelData
                            Layout.fillWidth: true
                            // The delegate uses a tinted selection, not a solid accent fill.
                            palette.highlightedText: AppTheme.text
                            text: modelData.title
                            highlighted: photoLibrary.mode === modelData.key
                            onClicked: root.showCollection(modelData.key)
                        }
                    }
                    Label { text: qsTr("FOLDERS"); color: AppTheme.mutedText; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.topMargin: 24; Layout.bottomMargin: 8 }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; model: photoLibrary.folders; spacing: 4
                        delegate: RowLayout {
                            required property var modelData
                            width: ListView.view.width; spacing: 0
                            KosItemDelegate {
                                Layout.fillWidth: true
                                palette.highlightedText: AppTheme.text
                                text: modelData.name || modelData.path
                                highlighted: photoLibrary.mode === "folder" && photoLibrary.folder.toString() === modelData.url.toString()
                                onClicked: root.showCollection("folder", modelData.url)
                            }
                            LiquidActionButton {
                                id: removeFolderButton
                                text: "×"; toolbarAction: true; materialForm: true; iconOnly: true
                                Accessible.name: qsTr("Remove folder from library; keep files")
                                onClicked: photoLibrary.removeFolder(modelData.url)
                                KosToolTip { target: removeFolderButton; text: qsTr("Remove from library. Files stay on disk.") }
                            }
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Your files stay where they are.")
                        color: AppTheme.mutedText; font.pixelSize: 11; wrapMode: Text.Wrap
                    }
                }
            }
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 22
                    spacing: 18; visible: !root.viewing
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            spacing: 4; Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: root.collectionTitle; elide: Text.ElideRight; color: AppTheme.text; font.pixelSize: 27; font.weight: Font.DemiBold; textFormat: Text.PlainText }
                            Label {
                                text: photoLibrary.loading ? qsTr("Loading…") : qsTr("%1 images").arg(photoLibrary.count)
                                color: AppTheme.mutedText; font.pixelSize: 12
                            }
                        }
                        LiquidActionButton {
                            text: photoLibrary.newestFirst ? qsTr("Newest first") : qsTr("By name")
                            materialForm: true
                            onClicked: photoLibrary.newestFirst = !photoLibrary.newestFirst
                        }
                        LiquidActionButton { text: qsTr("Refresh"); materialForm: true; onClicked: photoLibrary.refresh() }
                    }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        GridView {
                            id: grid
                            objectName: "photoGrid"
                            anchors.fill: parent; clip: true
                            model: photoLibrary
                            cellWidth: width / Math.max(1, Math.floor(width / 170))
                            cellHeight: cellWidth + 34
                            cacheBuffer: 0
                            currentIndex: photoLibrary.selectedIndex
                            activeFocusOnTab: true
                            Keys.onLeftPressed: photoLibrary.navigate(-1)
                            Keys.onRightPressed: photoLibrary.navigate(1)
                            Keys.onUpPressed: photoLibrary.navigate(-Math.max(1,Math.floor(width / 170)))
                            Keys.onDownPressed: photoLibrary.navigate(Math.max(1,Math.floor(width / 170)))
                            Keys.onReturnPressed: root.openSelected()
                            Keys.onEnterPressed: root.openSelected()
                            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
                            delegate: Item {
                                required property int index
                                required property string photoName
                                required property string imageSource
                                required property bool favorite
                                width: grid.cellWidth; height: grid.cellHeight
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 4; radius: 8
                                    color: photoLibrary.selectedIndex === index ? AppTheme.controlBackground : "transparent"
                                }
                                Image {
                                    id: thumbnail
                                    anchors.top: parent.top; anchors.topMargin: 12
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width - 24; height: width
                                    source: imageSource + "/thumb"; sourceSize: Qt.size(384,384)
                                    fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false
                                }
                                Label {
                                    anchors.centerIn: thumbnail
                                    visible: thumbnail.status === Image.Error
                                    text: qsTr("Cannot preview"); color: AppTheme.mutedText; font.pixelSize: 11
                                }
                                Label {
                                    anchors.top: thumbnail.bottom; anchors.topMargin: 8
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 12
                                    text: (favorite ? "★ " : "") + photoName
                                    color: AppTheme.text; font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle; textFormat: Text.PlainText
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { photoLibrary.select(index); grid.forceActiveFocus() }
                                    onDoubleClicked: { photoLibrary.select(index); root.openSelected() }
                                }
                                Accessible.role: Accessible.ListItem
                                Accessible.name: photoName
                                Accessible.selected: photoLibrary.selectedIndex === index
                                Accessible.onPressAction: { photoLibrary.select(index); root.openSelected() }
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(340,parent.width)
                            visible: photoLibrary.count === 0 && !photoLibrary.loading
                            spacing: 16
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                                text: photoLibrary.search ? qsTr("No matching images") : qsTr("A place for your photos")
                                color: AppTheme.text; font.pixelSize: 22
                            }
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                                text: photoLibrary.search ? qsTr("Try another filename.")
                                    : photoLibrary.mode === "favorites" ? qsTr("Mark an image as a favorite while viewing it.")
                                    : qsTr("Open images or add a folder to start browsing. Subfolders are added separately.")
                                color: AppTheme.mutedText
                            }
                            LiquidActionButton { Layout.alignment: Qt.AlignHCenter; text: qsTr("Add folder…"); materialForm: true; onClicked: folderDialog.open() }
                        }
                    }
                }
                PhotoViewer {
                    id: viewer
                    objectName: "photoViewer"
                    anchors.fill: parent; visible: root.viewing
                    photo: photoLibrary.selected
                    showInfo: root.infoVisible
                    position: photoLibrary.selectedIndex
                    total: photoLibrary.count
                    onPrevious: photoLibrary.navigate(-1)
                    onNext: photoLibrary.navigate(1)
                }
                DropArea {
                    anchors.fill: parent
                    onDropped: drop => { if (drop.hasUrls) { photoLibrary.openFiles(drop.urls); drop.acceptProposedAction() } }
                }
            }
        }
    }
}
