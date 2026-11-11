// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kos.Ui

Item {
    id: root
    property var frozenIds: []
    property string task: ""
    property string targetId: ""
    property string dateKey: ""
    property url targetUrl
    property url editSource
    readonly property bool anyOpen: nameDialog.visible || confirmDialog.visible || albumDialog.visible || filesDialog.visible || importDialog.visible || folderDialog.visible || exportDialog.visible || relinkDialog.visible || editDialog.visible || resultsDialog.visible
    function freeze(ids) { return Array.from(ids || photoLibrary.actionIds()) }
    function openFiles() { filesDialog.open() }
    function importFiles() { importDialog.open() }
    function addFolder() { folderDialog.open() }
    function exportItems(ids) { frozenIds = freeze(ids); exportDialog.open() }
    function editItem(photo) { editSource = photo.url; editDialog.open() }
    function relinkFolder(url) { targetUrl = url; relinkDialog.open() }
    function addToAlbum(ids) { frozenIds = freeze(ids); albumDialog.open() }
    function createAlbum(ids) { task = "createAlbum"; frozenIds = freeze(ids || []); nameField.text = ""; nameDialog.open() }
    function renameAlbum(album) { task = "renameAlbum"; targetId = album.id; nameField.text = album.name; nameDialog.open() }
    function createMemory(ids) { task = "createMemory"; frozenIds = freeze(ids || []); dateKey = ""; nameField.text = ""; nameDialog.open() }
    function createDateMemory(date, label) { task = "createDateMemory"; dateKey = date; nameField.text = label; nameDialog.open() }
    function confirm(action, ids, collection) {
        task = action; frozenIds = freeze(ids || []); targetId = collection ? collection.id : ""; targetUrl = collection ? collection.url || "" : ""
        confirmDialog.open()
    }
    function showResults() { resultsDialog.open() }
    function performConfirmed() {
        if (task === "trash") photoLibrary.moveToTrash(frozenIds)
        else if (task === "permanent") photoLibrary.permanentlyDelete(frozenIds)
        else if (task === "restore") photoLibrary.restore(frozenIds)
        else if (task === "deleteAlbum") photoLibrary.deleteAlbum(targetId)
        else if (task === "deleteMemory") photoLibrary.deleteMemory(targetId)
        else if (task === "removeFolder") photoLibrary.removeFolder(targetUrl)
        else if (task === "removeFromAlbum") photoLibrary.removeFromAlbum(targetId, frozenIds)
    }
    FileDialog { id: filesDialog; title: qsTr("Open photos and videos temporarily"); fileMode: FileDialog.OpenFiles; nameFilters: photoLibrary.fileFilters; onAccepted: photoLibrary.openFiles(selectedFiles) }
    FileDialog { id: importDialog; title: qsTr("Import copies into your library"); fileMode: FileDialog.OpenFiles; nameFilters: photoLibrary.fileFilters; onAccepted: photoLibrary.importFiles(selectedFiles) }
    FolderDialog { id: folderDialog; title: qsTr("Add a referenced folder — originals stay in place"); onAccepted: photoLibrary.addFolder(selectedFolder) }
    FolderDialog { id: exportDialog; title: qsTr("Export original files to a folder"); onAccepted: photoLibrary.exportOriginals(root.frozenIds, selectedFolder) }
    FolderDialog { id: relinkDialog; title: qsTr("Locate this folder in its new location"); onAccepted: photoLibrary.relinkFolder(root.targetUrl, selectedFolder) }
    FolderDialog { id: editDialog; title: qsTr("Choose where to save the editing copy"); onAccepted: editorBridge.editCopy(root.editSource, selectedFolder) }
    LibraryDialog {
        id: nameDialog; objectName: "collectionNameDialog"
        title: root.task === "renameAlbum" ? qsTr("Rename album") : root.task === "createAlbum" ? qsTr("New album") : qsTr("New memory")
        onOpened: { nameField.forceActiveFocus(); nameField.selectAll() }
        contentItem: ColumnLayout {
            spacing: 16
            LiquidTextField { id: nameField; objectName: "collectionNameField"; Layout.fillWidth: true; materialForm: true; placeholderText: qsTr("Name"); Accessible.name: qsTr("Collection name"); onAccepted: if (text.trim()) saveName.clicked() }
            Label { Layout.fillWidth: true; visible: root.task !== "renameAlbum"; text: root.task === "createDateMemory" ? qsTr("Use photos and videos from %1. Hidden and deleted items are excluded.").arg(root.dateKey) : qsTr("%1 selected items will be included. You can add more later.").arg(root.frozenIds.length); wrapMode: Text.Wrap; color: AppTheme.mutedText; font.pixelSize: 12 }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                LiquidActionButton { text: qsTr("Cancel"); toolbarAction: true; materialForm: true; flat: true; onClicked: nameDialog.close() }
                LiquidActionButton {
                    id: saveName; text: root.task === "renameAlbum" ? qsTr("Save") : qsTr("Create"); toolbarAction: true; materialForm: true; enabled: !!nameField.text.trim()
                    onClicked: {
                        const name = nameField.text.trim()
                        if (root.task === "createAlbum") photoLibrary.createAlbum(name, root.frozenIds)
                        else if (root.task === "renameAlbum") photoLibrary.renameAlbum(root.targetId, name)
                        else if (root.task === "createDateMemory") photoLibrary.createDateMemory(root.dateKey, name)
                        else photoLibrary.createMemory(name, root.frozenIds)
                        nameDialog.close()
                    }
                }
            }
        }
    }
    LibraryDialog {
        id: confirmDialog; objectName: "libraryConfirmation"
        title: root.task === "restore" ? qsTr("Restore %1 items?").arg(root.frozenIds.length) : root.task === "permanent" ? qsTr("Permanently remove %1 items?").arg(root.frozenIds.length) : root.task === "trash" ? qsTr("Delete %1 items from the library?").arg(root.frozenIds.length) : root.task === "deleteAlbum" ? qsTr("Delete this album?") : root.task === "deleteMemory" ? qsTr("Delete this memory?") : root.task === "removeFolder" ? qsTr("Remove this folder?") : qsTr("Remove items from this album?")
        contentItem: ColumnLayout {
            spacing: 18
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: AppTheme.text; font.pixelSize: 13
                text: root.task === "restore" ? qsTr("Return these items to your library. Imported copies are recovered from the app’s recovery area. Referenced external originals are not moved or changed.") : root.task === "permanent" ? qsTr("This cannot be undone. Files owned by NkPhotos will be permanently deleted. Referenced external originals will remain untouched; only their library records are removed.")
                    : root.task === "trash" ? qsTr("You can restore these items from Recently deleted for 30 days. Imported files move to the app’s recovery area. Referenced external originals stay in their original location and are never deleted.")
                    : root.task === "removeFolder" ? qsTr("Remove this folder’s library reference. Original files stay on disk. Items also referenced by another folder remain in the library.")
                    : root.task === "deleteMemory" ? qsTr("The memory and its playback settings will be removed. Its photos and videos stay in the library.")
                    : qsTr("Your photos and videos stay in the library. Original files are not changed.")
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                LiquidActionButton { text: qsTr("Cancel"); toolbarAction: true; materialForm: true; flat: true; onClicked: confirmDialog.close() }
                LiquidActionButton { text: root.task === "restore" ? qsTr("Restore") : root.task === "permanent" ? qsTr("Remove permanently") : qsTr("Remove"); toolbarAction: true; materialForm: true; onClicked: { root.performConfirmed(); confirmDialog.close() } }
            }
        }
    }
    LibraryDialog {
        id: albumDialog; objectName: "addToAlbumDialog"; title: qsTr("Add to album")
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("Choose an album for %1 items.").arg(root.frozenIds.length); wrapMode: Text.Wrap; color: AppTheme.mutedText }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: Math.min(240, Math.max(48, count * 40)); clip: true; model: photoLibrary.albums
                delegate: LibraryRow { required property var modelData; width: ListView.view.width; text: modelData.name; symbol: "rectangle_stack"; onClicked: { photoLibrary.addToAlbum(modelData.id, root.frozenIds); albumDialog.close() } }
                ScrollBar.vertical: ScrollBar {}
            }
            RowLayout {
                Layout.fillWidth: true
                LiquidActionButton { text: qsTr("New album…"); toolbarAction: true; materialForm: true; onClicked: { const ids = root.frozenIds.slice(); albumDialog.close(); root.createAlbum(ids) } }
                Item { Layout.fillWidth: true }
                LiquidActionButton { text: qsTr("Cancel"); toolbarAction: true; materialForm: true; flat: true; onClicked: albumDialog.close() }
            }
        }
    }
    LibraryDialog {
        id: resultsDialog; objectName: "operationResultsDialog"; title: qsTr("Operation results")
        contentItem: ColumnLayout {
            spacing: 12
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: Math.min(280, Math.max(60, count * 58)); clip: true; model: photoLibrary.operationResults
                delegate: Label { required property var modelData; width: ListView.view.width; padding: 8; text: (modelData.name || modelData.path || modelData.url || modelData.id || "") + "\n" + (modelData.message || modelData.error || modelData.status || ""); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: AppTheme.text; font.pixelSize: 12 }
                ScrollBar.vertical: ScrollBar {}
            }
            LiquidActionButton { Layout.alignment: Qt.AlignRight; text: qsTr("Done"); toolbarAction: true; materialForm: true; onClicked: resultsDialog.close() }
        }
    }
}
