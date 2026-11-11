// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

FocusScope {
    id: root
    property int tileSize: 170
    property bool showNames: false
    property bool cropThumbnails: true
    property bool selectionMode: false
    property alias grid: grid
    readonly property bool grouped: photoLibrary.dateView !== "all" && photoLibrary.count > 0
    readonly property string queryKey: photoLibrary.mode + ":" + photoLibrary.collectionId + ":" + photoLibrary.folder.toString() + ":" + photoLibrary.search + ":" + photoLibrary.dateFilter + ":" + photoLibrary.mediaFilter
    property string displayedKey: ""
    property string resetKey: ""
    property real resetY: 0
    Connections {
        target: photoLibrary
        function onModelAboutToBeReset() { root.resetY = grid.contentY; root.resetKey = root.displayedKey }
        function onModelReset() {
            const retain = root.resetKey === root.queryKey
            const y = root.resetY
            root.displayedKey = root.queryKey
            if (retain) Qt.callLater(function() { grid.contentY = Math.max(0,Math.min(y,Math.max(0,grid.contentHeight-grid.height))) })
        }
        function onChanged() { root.displayedKey = root.queryKey }
    }
    signal openSelected()
    signal addFolder()
    signal openFiles()
    signal importFiles()
    signal actionsRequested(Item anchor)
    signal createDateMemory(string key, string label)
    signal collectionActions(Item anchor)
    signal playMemory()
    function moveSelection(delta, modifiers) {
        const target = Math.max(0, Math.min(photoLibrary.count - 1, photoLibrary.selectedIndex + delta))
        if (modifiers & Qt.ShiftModifier) photoLibrary.selectForBatch(target, modifiers)
        else photoLibrary.selectForBatch(target, 0)
        grid.positionViewAtIndex(photoLibrary.selectedIndex, GridView.Contain)
    }
    function durationLabel(seconds) { const n = Math.max(0, Math.floor((seconds || 0) / 1000)); return Math.floor(n / 60) + ":" + (n % 60 < 10 ? "0" : "") + n % 60 }
    ColumnLayout {
        anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 50; spacing: 10
            Label { Layout.fillWidth: true; text: photoLibrary.loading ? qsTr("Loading…") : qsTr("%1 items").arg(photoLibrary.count); color: AppTheme.mutedText; font.pixelSize: 12; elide: Text.ElideRight }
            LiquidSegmentedControl {
                objectName: "dateViewSelector"
                Layout.preferredWidth: Math.min(266, root.width * .55); implicitHeight: 30
                visible: photoLibrary.mode !== "trash" && photoLibrary.mode !== "memory"
                materialForm: true; labels: [qsTr("Years"),qsTr("Months"),qsTr("Days"),qsTr("All")]
                currentIndex: ["years","months","days","all"].indexOf(photoLibrary.dateView)
                onSelectionRequested: index => photoLibrary.dateView = ["years","months","days","all"][index]
            }
            PhotoAction { visible: photoLibrary.mode === "memory"; label: qsTr("Play memory"); iconName: "play_fill"; enabled: photoLibrary.count > 0; onClicked: root.playMemory() }
            PhotoAction { id: collectionButton; visible: photoLibrary.mode === "album" || photoLibrary.mode === "memory"; label: qsTr("Collection actions"); iconName: "ellipsis"; onClicked: root.collectionActions(collectionButton) }
        }
        RowLayout {
            visible: !!photoLibrary.dateFilter; Layout.fillWidth: true; Layout.bottomMargin: 8
            Label { Layout.fillWidth: true; text: photoLibrary.dateFilter; color: AppTheme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            LiquidActionButton { text: qsTr("All dates"); toolbarAction: true; materialForm: true; flat: true; onClicked: photoLibrary.dateFilter = "" }
        }
        Label {
            Layout.fillWidth: true; Layout.bottomMargin: 12; visible: photoLibrary.mode === "trash" || photoLibrary.mode === "hidden"
            text: photoLibrary.mode === "trash" ? qsTr("Items remain recoverable for 30 days. External originals are never deleted.") : qsTr("Hidden items are excluded from your library and memory suggestions. This view is not password protected.")
            color: AppTheme.mutedText; font.pixelSize: 12; wrapMode: Text.Wrap
        }
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            GridView {
                id: grid
                objectName: "photoGrid"
                anchors.fill: parent; clip: true; visible: !root.grouped
                model: photoLibrary
                readonly property int columns: Math.max(1, Math.floor(width / root.tileSize))
                cellWidth: width / columns; cellHeight: cellWidth + (root.showNames ? 25 : 0)
                cacheBuffer: 0; currentIndex: photoLibrary.selectedIndex
                activeFocusOnTab: true; keyNavigationEnabled: false
                Keys.onLeftPressed: event => root.moveSelection(-1, event.modifiers)
                Keys.onRightPressed: event => root.moveSelection(1, event.modifiers)
                Keys.onUpPressed: event => root.moveSelection(-columns, event.modifiers)
                Keys.onDownPressed: event => root.moveSelection(columns, event.modifiers)
                Keys.onReturnPressed: root.openSelected()
                Keys.onEnterPressed: root.openSelected()
                Keys.onSpacePressed: { if (photoLibrary.selectedIndex >= 0) photoLibrary.selectForBatch(photoLibrary.selectedIndex, Qt.ControlModifier) }
                delegate: Item {
                    id: tile
                    required property int index
                    required property var model
                    readonly property bool selected: !!model.batchSelected
                    readonly property bool focused: photoLibrary.selectedIndex === index
                    width: grid.cellWidth; height: grid.cellHeight
                    Rectangle {
                        id: frame
                        x: 2; y: 2; width: parent.width - 4; height: width
                        color: AppTheme.contentSecondaryBackground; clip: true
                        Image {
                            id: thumbnail; anchors.fill: parent
                            source: tile.model.imageSource ? tile.model.imageSource + "/thumb" : ""; sourceSize: Qt.size(384,384)
                            fillMode: root.cropThumbnails ? Image.PreserveAspectCrop : Image.PreserveAspectFit
                            asynchronous: true; cache: false
                        }
                        Column {
                            anchors.centerIn: parent; spacing: 8
                            visible: thumbnail.status === Image.Error || !tile.model.available
                            VectorIcon { anchors.horizontalCenter: parent.horizontalCenter; name: tile.model.mediaType === "video" ? "play_rectangle" : "photo"; size: 22; color: AppTheme.mutedText }
                            Label { text: tile.model.available ? qsTr("Cannot preview") : qsTr("File unavailable"); color: AppTheme.mutedText; font.pixelSize: 11 }
                        }
                        Rectangle { anchors.fill: parent; color: AppTheme.text; opacity: pointer.containsMouse && !tile.selected ? .06 : 0 }
                        Rectangle { anchors.fill: parent; visible: tile.selected || tile.focused; color: "transparent"; border.width: tile.selected ? 3 : 2; border.color: tile.selected ? AppTheme.accent : AppTheme.withAlpha(AppTheme.accent,.65) }
                        Rectangle {
                            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
                            width: 22; height: 22; radius: 11; visible: root.selectionMode || tile.selected
                            color: tile.selected ? AppTheme.accent : "#80000000"; border.width: 1; border.color: "white"
                            VectorIcon { anchors.centerIn: parent; name: "checkmark"; visible: tile.selected; size: 12; color: "white" }
                        }
                        Rectangle {
                            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 8
                            width: 22; height: 22; radius: 11; color: "#80000000"; visible: tile.model.favorite
                            VectorIcon { anchors.centerIn: parent; name: "heart"; filled: true; size: 12; color: "white" }
                        }
                        Rectangle {
                            visible: tile.model.mediaType === "video"; anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 8
                            width: duration.implicitWidth + 16; height: 23; radius: 6; color: "#99000000"
                            Label { id: duration; anchors.centerIn: parent; text: root.durationLabel(tile.model.duration); color: "white"; font.pixelSize: 11 }
                        }
                    }
                    Label { visible: root.showNames; anchors.top: frame.bottom; anchors.topMargin: 5; anchors.left: frame.left; anchors.right: frame.right; text: tile.model.title || tile.model.photoName; color: AppTheme.text; font.pixelSize: 11; elide: Text.ElideMiddle; horizontalAlignment: Text.AlignHCenter; textFormat: Text.PlainText }
                    MouseArea {
                        id: pointer; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => {
                            if (mouse.button === Qt.RightButton) {
                                if (!tile.selected) photoLibrary.selectForBatch(tile.index, 0)
                                root.actionsRequested(tile)
                            } else if (root.selectionMode || mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) {
                                photoLibrary.selectForBatch(tile.index, root.selectionMode && !(mouse.modifiers & Qt.ShiftModifier) ? Qt.ControlModifier : mouse.modifiers)
                            } else photoLibrary.selectForBatch(tile.index, 0)
                            grid.forceActiveFocus()
                        }
                        onDoubleClicked: mouse => { if (mouse.button === Qt.LeftButton && !root.selectionMode && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) { photoLibrary.select(tile.index); root.openSelected() } }
                    }
                    KosToolTip { target: tile; hovered: pointer.containsMouse; showOnFocus: false; when: !root.showNames; text: tile.model.title || tile.model.photoName }
                    Accessible.role: Accessible.ListItem; Accessible.name: tile.model.title || tile.model.photoName; Accessible.selected: tile.selected
                    Accessible.onPressAction: { photoLibrary.select(tile.index); root.openSelected() }
                }
                ScrollBar.vertical: ScrollBar {}
            }
            DateBrowser {
                anchors.fill: parent; visible: root.grouped
                onOpenDate: key => { photoLibrary.dateFilter = key; photoLibrary.dateView = photoLibrary.dateView === "years" ? "months" : photoLibrary.dateView === "months" ? "days" : "all" }
                onCreateMemory: (key, label) => root.createDateMemory(key, label)
            }
            ColumnLayout {
                anchors.centerIn: parent; width: Math.min(340,parent.width - 24); spacing: 16
                visible: photoLibrary.count === 0 && !photoLibrary.loading
                VectorIcon { Layout.alignment: Qt.AlignHCenter; name: photoLibrary.mode === "favorites" ? "heart" : photoLibrary.mode === "trash" ? "trash" : "photo_on_rectangle"; size: 42; color: AppTheme.mutedText }
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: photoLibrary.search ? qsTr("No matching photos") : photoLibrary.mode === "favorites" ? qsTr("Your favorite moments") : photoLibrary.mode === "trash" ? qsTr("Nothing recently deleted") : photoLibrary.mode === "hidden" ? qsTr("No hidden items") : photoLibrary.mode === "album" ? qsTr("An album waiting for photos") : photoLibrary.mode === "memory" ? qsTr("This memory is empty") : photoLibrary.mode === "recent" ? qsTr("Your imports appear here") : photoLibrary.mediaFilter === "video" ? qsTr("No videos yet") : qsTr("Make room for your photos")
                    color: AppTheme.text; font.pixelSize: 21; font.weight: Font.Medium; wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                    text: photoLibrary.search ? qsTr("Search names, titles, descriptions, keywords or dates.") : photoLibrary.mode === "favorites" ? qsTr("Choose the heart on any photo or video to find it here.") : photoLibrary.mode === "album" ? qsTr("Select items in your library, then choose Add to album.") : photoLibrary.mode === "memory" ? qsTr("Edit this memory to choose its photos and videos.") : photoLibrary.mode === "trash" ? qsTr("Deleted library items can be restored here before they expire.") : photoLibrary.mode === "hidden" ? qsTr("Items you hide are kept out of other collections.") : qsTr("Import a copy, or add a folder to keep your files where they are.")
                    color: AppTheme.mutedText; font.pixelSize: 13
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter; visible: !photoLibrary.search && ["all","recent","folder","session"].indexOf(photoLibrary.mode) >= 0
                    LiquidActionButton { text: qsTr("Import…"); toolbarAction: true; materialForm: true; onClicked: root.importFiles() }
                    LiquidActionButton { text: qsTr("Add folder…"); toolbarAction: true; materialForm: true; flat: true; onClicked: root.addFolder() }
                }
            }
            BusyIndicator { anchors.centerIn: parent; visible: photoLibrary.loading && photoLibrary.count === 0; running: visible }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 42; spacing: 8
            Label {
                Layout.fillWidth: true
                text: photoLibrary.selectionCount > 0 ? qsTr("%1 selected").arg(photoLibrary.selectionCount) : photoLibrary.selected.title || photoLibrary.selected.name || ""
                textFormat: Text.PlainText; elide: Text.ElideMiddle; color: photoLibrary.selectionCount > 0 ? AppTheme.text : AppTheme.mutedText; font.pixelSize: 12
            }
            LiquidActionButton { visible: photoLibrary.selectionCount > 0; text: qsTr("Clear"); toolbarAction: true; materialForm: true; flat: true; onClicked: photoLibrary.clearSelection() }
            LiquidActionButton { id: selectionActions; visible: photoLibrary.selectionCount > 0; text: qsTr("Actions…"); toolbarAction: true; materialForm: true; onClicked: root.actionsRequested(selectionActions) }
            LiquidActionButton { visible: photoLibrary.selectionCount === 0 && photoLibrary.count > 0; text: root.selectionMode ? qsTr("Done") : qsTr("Select"); toolbarAction: true; materialForm: true; flat: true; onClicked: root.selectionMode = !root.selectionMode }
        }
    }
}
