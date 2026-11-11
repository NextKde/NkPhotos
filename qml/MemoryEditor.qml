// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kos.Ui

LibraryDialog {
    id: root
    objectName: "memoryEditor"
    property string memoryId: ""
    property var itemIds: []
    property string coverId: ""
    property url musicUrl
    property var returnState: ({})
    property var pickingIds: []
    readonly property bool pickerOpen: picker.visible || musicDialog.visible
    title: qsTr("Edit memory")
    width: Math.min(580, parent ? parent.width - 32 : 580)
    function edit(memory) {
        memoryId = memory.id; itemIds = Array.from(memory.ids || []); coverId = memory.coverId || ""
        nameField.text = memory.name || ""; seconds.value = Math.max(1, Math.round(memory.seconds || 5))
        musicUrl = memory.music || ""; muted.checked = !!memory.muted; open()
    }
    function moveItem(index, delta) {
        const ids = itemIds.slice(); const target = index + delta
        if (target < 0 || target >= ids.length) return
        const value = ids.splice(index,1)[0]; ids.splice(target,0,value); itemIds = ids
    }
    function removeItem(index) {
        const ids = itemIds.slice(); const removed = ids.splice(index,1)[0]; itemIds = ids
        if (coverId === removed) coverId = ids.length ? ids[0] : ""
    }
    function beginPicking() {
        returnState = {mode:photoLibrary.mode, folder:photoLibrary.folder, collectionId:photoLibrary.collectionId, search:photoLibrary.search, dateFilter:photoLibrary.dateFilter, dateView:photoLibrary.dateView, mediaFilter:photoLibrary.mediaFilter, selectedId:photoLibrary.selected.mediaId || photoLibrary.selected.id || "", selectedIds:Array.from(photoLibrary.selectedIds)}
        pickingIds = []; pickerSearch.text = ""
        photoLibrary.showCollection("all"); photoLibrary.search = ""; photoLibrary.dateFilter = ""; photoLibrary.mediaFilter = "all"; photoLibrary.dateView = "all"
        picker.open()
    }
    function finishPicking() {
        const state = returnState
        if (state.mode === "album") photoLibrary.showAlbum(state.collectionId)
        else if (state.mode === "memory") photoLibrary.showMemory(state.collectionId)
        else photoLibrary.showCollection(state.mode, state.folder)
        photoLibrary.search = state.search; photoLibrary.dateFilter = state.dateFilter; photoLibrary.dateView = state.dateView; photoLibrary.mediaFilter = state.mediaFilter
        photoLibrary.clearSelection()
        for (const id of state.selectedIds) {
            photoLibrary.selectById(id)
            if (photoLibrary.selected.mediaId === id) photoLibrary.selectForBatch(photoLibrary.selectedIndex,Qt.ControlModifier)
        }
        if (state.selectedId) photoLibrary.selectById(state.selectedId)
    }
    contentItem: ScrollView {
        clip: true; contentWidth: availableWidth
        implicitHeight: editorContents.implicitHeight
        ColumnLayout {
        id: editorContents
        width: parent.width; spacing: 12
        LiquidTextField { id: nameField; Layout.fillWidth: true; materialForm: true; placeholderText: qsTr("Memory name"); Accessible.name: qsTr("Memory name") }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Seconds per photo"); color: AppTheme.text; font.pixelSize: 12 }
            SpinBox { id: seconds; from: 1; to: 60; editable: true; value: 5; Accessible.name: qsTr("Seconds per photo") }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("%1 items").arg(root.itemIds.length); color: AppTheme.mutedText; font.pixelSize: 12 }
        }
        ListView {
            id: itemList; objectName: "memoryItemList"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(230, Math.max(64, root.itemIds.length * 60)); clip: true
            model: root.itemIds
            delegate: RowLayout {
                id: memoryRow
                required property string modelData
                required property int index
                readonly property var entry: photoLibrary.entry(modelData)
                width: itemList.width; height: 60; spacing: 7
                Image { Layout.preferredWidth: 50; Layout.preferredHeight: 50; source: memoryRow.entry.imageSource ? memoryRow.entry.imageSource + "/thumb" : ""; sourceSize: Qt.size(100,100); asynchronous: true; fillMode: Image.PreserveAspectCrop }
                Label { Layout.fillWidth: true; text: memoryRow.entry.title || memoryRow.entry.name || qsTr("Unavailable item"); textFormat: Text.PlainText; elide: Text.ElideMiddle; color: AppTheme.text; font.pixelSize: 12 }
                PhotoAction { label: qsTr("Use as cover"); iconName: root.coverId === memoryRow.modelData ? "checkmark" : "photo"; onClicked: root.coverId = memoryRow.modelData }
                PhotoAction { label: qsTr("Move earlier"); iconName: "chevron_up"; enabled: memoryRow.index > 0; onClicked: root.moveItem(memoryRow.index,-1) }
                PhotoAction { label: qsTr("Move later"); iconName: "chevron_down"; enabled: memoryRow.index + 1 < root.itemIds.length; onClicked: root.moveItem(memoryRow.index,1) }
                PhotoAction { label: qsTr("Remove from memory"); iconName: "xmark"; onClicked: root.removeItem(memoryRow.index) }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        RowLayout {
            Layout.fillWidth: true
            LiquidActionButton { text: qsTr("Add items…"); toolbarAction: true; materialForm: true; flat: true; onClicked: root.beginPicking() }
            Item { Layout.fillWidth: true }
            LiquidActionButton { text: qsTr("Choose music…"); toolbarAction: true; materialForm: true; flat: true; onClicked: musicDialog.open() }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; text: root.musicUrl.toString() ? decodeURIComponent(root.musicUrl.toString().split("/").pop()) : qsTr("No background music"); textFormat: Text.PlainText; elide: Text.ElideMiddle; color: AppTheme.mutedText; font.pixelSize: 11 }
            PhotoAction { visible: !!root.musicUrl.toString(); label: qsTr("Remove music"); iconName: "xmark"; onClicked: root.musicUrl = "" }
            CheckBox { id: muted; text: qsTr("Mute music") }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            LiquidActionButton { text: qsTr("Cancel"); toolbarAction: true; materialForm: true; flat: true; onClicked: root.close() }
            LiquidActionButton { text: qsTr("Save memory"); toolbarAction: true; materialForm: true; enabled: !!nameField.text.trim(); onClicked: { photoLibrary.updateMemory(root.memoryId,nameField.text.trim(),root.itemIds,root.coverId,seconds.value,root.musicUrl,muted.checked); root.close() } }
        }
    }
    }
    FileDialog { id: musicDialog; title: qsTr("Choose local music"); fileMode: FileDialog.OpenFile; nameFilters: [qsTr("Audio files (*.mp3 *.m4a *.aac *.ogg *.opus *.flac *.wav)"),qsTr("All files (*)")]; onAccepted: root.musicUrl = selectedFile }
    LibraryDialog {
        id: picker; objectName: "memoryItemPicker"; title: qsTr("Add to memory")
        width: Math.min(620, parent ? parent.width - 32 : 620)
        onClosed: root.finishPicking()
        contentItem: ColumnLayout {
            spacing: 12
            LiquidTextField { id: pickerSearch; Layout.fillWidth: true; materialForm: true; placeholderText: qsTr("Search your library"); onTextEdited: photoLibrary.search = text }
            GridView {
                id: pickerGrid
                Layout.fillWidth: true; Layout.preferredHeight: Math.min(260, root.parent ? root.parent.height - 220 : 260)
                clip: true; model: photoLibrary; cellWidth: width / Math.max(1, Math.floor(width / 110)); cellHeight: cellWidth
                delegate: Item {
                    id: pickTile
                    required property var model
                    width: pickerGrid.cellWidth; height: pickerGrid.cellHeight
                    readonly property string mediaId: model.mediaId
                    readonly property bool already: root.itemIds.indexOf(mediaId) >= 0
                    readonly property bool chosen: root.pickingIds.indexOf(mediaId) >= 0
                    Image { anchors.fill: parent; anchors.margins: 3; source: pickTile.model.imageSource ? pickTile.model.imageSource + "/thumb" : ""; sourceSize: Qt.size(200,200); asynchronous: true; fillMode: Image.PreserveAspectCrop; opacity: pickTile.already ? .45 : 1 }
                    Rectangle { anchors.fill: parent; anchors.margins: 3; color: "transparent"; border.width: pickTile.chosen ? 3 : 0; border.color: AppTheme.accent }
                    VectorIcon { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 9; visible: pickTile.chosen || pickTile.already; name: "checkmark_circle"; size: 20; color: "white" }
                    MouseArea { anchors.fill: parent; enabled: !pickTile.already; onClicked: { const ids = root.pickingIds.slice(); const i = ids.indexOf(pickTile.mediaId); if (i >= 0) ids.splice(i,1); else ids.push(pickTile.mediaId); root.pickingIds = ids } }
                    Accessible.role: Accessible.CheckBox; Accessible.name: model.photoName; Accessible.checked: chosen || already
                }
                ScrollBar.vertical: ScrollBar {}
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                LiquidActionButton { text: qsTr("Cancel"); toolbarAction: true; materialForm: true; flat: true; onClicked: picker.close() }
                LiquidActionButton { text: qsTr("Add %1 items").arg(root.pickingIds.length); toolbarAction: true; materialForm: true; enabled: root.pickingIds.length > 0; onClicked: { root.itemIds = root.itemIds.concat(root.pickingIds); if (!root.coverId && root.itemIds.length) root.coverId = root.itemIds[0]; picker.close() } }
            }
        }
    }
}
