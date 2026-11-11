// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import Kos.Ui

LiquidPopover {
    id: root
    required property var dialogs
    property var frozenIds: []
    property var firstEntry: ({})
    property string collectionId: ""
    property string collectionMode: ""
    property bool allFavorite: false
    property bool allHidden: false
    property bool hasTemporary: false
    property var importUrls: []
    readonly property bool canOrganize: frozenIds.length > 0 && !hasTemporary
    signal showInformation(string mediaId)
    signal editPhoto(var photo)
    function show(anchor, ids) {
        frozenIds = Array.from(ids || photoLibrary.actionIds())
        firstEntry = frozenIds.length ? photoLibrary.entry(frozenIds[0]) : ({})
        hasTemporary = frozenIds.some(id => !!photoLibrary.entry(id).temporary)
        importUrls = frozenIds.map(id => photoLibrary.entry(id)).filter(e => e.temporary && e.available).map(e => e.url)
        collectionId = photoLibrary.collectionId; collectionMode = photoLibrary.mode
        allFavorite = frozenIds.length > 0 && frozenIds.every(id => photoLibrary.entry(id).favorite)
        allHidden = frozenIds.length > 0 && frozenIds.every(id => photoLibrary.entry(id).hidden)
        anchorItem = anchor; openAtAnchor()
    }
    LiquidPopoverItem { text: qsTr("Import selected copies…"); iconName: "square_arrow_down"; visible: root.hasTemporary; enabled: root.importUrls.length > 0 && !photoLibrary.busy; onTriggered: photoLibrary.importFiles(root.importUrls) }
    LiquidPopoverItem { text: root.allFavorite ? qsTr("Remove from favorites") : qsTr("Add to favorites"); iconName: "heart"; visible: root.collectionMode !== "trash"; enabled: root.canOrganize; onTriggered: photoLibrary.setFavorite(root.frozenIds, !root.allFavorite) }
    LiquidPopoverItem { text: qsTr("Add to album…"); iconName: "rectangle_stack"; visible: root.collectionMode !== "trash"; enabled: root.canOrganize; onTriggered: root.dialogs.addToAlbum(root.frozenIds) }
    LiquidPopoverItem { text: qsTr("Create memory…"); iconName: "play_circle"; visible: root.collectionMode !== "trash" && root.collectionMode !== "hidden"; enabled: root.canOrganize; onTriggered: root.dialogs.createMemory(root.frozenIds) }
    LiquidPopoverItem { text: qsTr("Use as album cover"); iconName: "photo"; visible: root.collectionMode === "album"; enabled: root.frozenIds.length === 1 && root.canOrganize; onTriggered: photoLibrary.setAlbumCover(root.collectionId,root.frozenIds[0]) }
    LiquidPopoverItem { text: qsTr("Remove from this album…"); visible: root.collectionMode === "album"; enabled: root.canOrganize; onTriggered: root.dialogs.confirm("removeFromAlbum",root.frozenIds,{id:root.collectionId}) }
    MenuSeparator {}
    LiquidPopoverItem { text: qsTr("Information…"); iconName: "info_circle"; enabled: root.frozenIds.length === 1; onTriggered: root.showInformation(root.frozenIds[0]) }
    LiquidPopoverItem { text: qsTr("Edit a copy in NkShot…"); visible: root.collectionMode !== "trash"; enabled: root.frozenIds.length === 1 && root.firstEntry.mediaType === "photo" && root.firstEntry.available && !editorBridge.busy; onTriggered: root.editPhoto(root.firstEntry) }
    LiquidPopoverItem { text: qsTr("Open with…"); visible: root.collectionMode !== "trash"; enabled: root.frozenIds.length === 1 && root.firstEntry.available && !shareBridge.busy; onTriggered: shareBridge.openWith(root.firstEntry.url) }
    LiquidPopoverItem { text: qsTr("Export originals…"); iconName: "square_arrow_up"; visible: root.collectionMode !== "trash"; enabled: root.canOrganize && !photoLibrary.busy; onTriggered: root.dialogs.exportItems(root.frozenIds) }
    MenuSeparator {}
    LiquidPopoverItem { text: root.allHidden ? qsTr("Unhide") : qsTr("Hide"); iconName: "eye_slash"; visible: root.collectionMode !== "trash"; enabled: root.canOrganize; onTriggered: photoLibrary.setHidden(root.frozenIds,!root.allHidden) }
    LiquidPopoverItem { text: qsTr("Move to Recently deleted…"); iconName: "trash"; visible: root.collectionMode !== "trash"; enabled: root.canOrganize; onTriggered: root.dialogs.confirm("trash",root.frozenIds) }
    LiquidPopoverItem { text: qsTr("Restore"); iconName: "arrow_counterclockwise"; visible: root.collectionMode === "trash"; enabled: root.canOrganize; onTriggered: root.dialogs.confirm("restore",root.frozenIds) }
    LiquidPopoverItem { text: qsTr("Remove permanently…"); iconName: "trash"; visible: root.collectionMode === "trash"; enabled: root.canOrganize; onTriggered: root.dialogs.confirm("permanent",root.frozenIds) }
}
