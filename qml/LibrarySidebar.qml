// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

Rectangle {
    id: root
    property string browserPage: "library"
    signal navigate(string mode, url folder)
    signal browse(string page)
    signal addFolder()
    signal folderActions(var folder, Item anchor)
    // The sidebar column is a flush surface owned by the shell (see
    // Main.qml's sidebarPlane); this control only carries the rows.
    color: "transparent"
    ScrollView {
        anchors.fill: parent; anchors.margins: 10
        // ScrollView paints an opaque palette-window background by default,
        // which would cover the surface underneath.
        background: null
        clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 2
            Label { text: qsTr("Library"); color: AppTheme.mutedText; font.pixelSize: 11; Layout.leftMargin: 12; Layout.topMargin: 8; Layout.bottomMargin: 8 }
            Repeater {
                model: [
                    {key:"all",title:qsTr("All photos"),symbol:"photo_on_rectangle"},
                    {key:"favorites",title:qsTr("Favorites"),symbol:"heart"},
                    {key:"recent",title:qsTr("Recent imports"),symbol:"clock"},
                    {key:"videos",title:qsTr("Videos"),symbol:"play_rectangle"},
                    {key:"session",title:qsTr("Opened files"),symbol:"photo"}
                ]
                delegate: LibraryRow {
                    required property var modelData
                    Layout.fillWidth: true; text: modelData.title; symbol: modelData.symbol
                    highlighted: root.browserPage === "library" && (modelData.key === "videos" ? photoLibrary.mode === "all" && photoLibrary.mediaFilter === "video" : photoLibrary.mode === modelData.key && photoLibrary.mediaFilter !== "video")
                    onClicked: root.navigate(modelData.key, "")
                }
            }
            Label { text: qsTr("Collections"); color: AppTheme.mutedText; font.pixelSize: 11; Layout.leftMargin: 12; Layout.topMargin: 22; Layout.bottomMargin: 8 }
            LibraryRow { Layout.fillWidth: true; text: qsTr("Albums"); symbol: "rectangle_stack"; highlighted: root.browserPage === "albums" || (root.browserPage === "library" && photoLibrary.mode === "album"); onClicked: root.browse("albums") }
            LibraryRow { Layout.fillWidth: true; text: qsTr("Memories"); symbol: "play_circle"; highlighted: root.browserPage === "memories" || (root.browserPage === "library" && photoLibrary.mode === "memory"); onClicked: root.browse("memories") }
            LibraryRow { Layout.fillWidth: true; text: qsTr("Hidden"); symbol: "eye_slash"; highlighted: root.browserPage === "library" && photoLibrary.mode === "hidden"; onClicked: root.navigate("hidden", "") }
            LibraryRow { Layout.fillWidth: true; text: qsTr("Recently deleted"); symbol: "trash"; highlighted: root.browserPage === "library" && photoLibrary.mode === "trash"; onClicked: root.navigate("trash", "") }
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 18; Layout.bottomMargin: 3
                Label { Layout.fillWidth: true; Layout.leftMargin: 12; text: qsTr("Folders"); color: AppTheme.mutedText; font.pixelSize: 11 }
                PhotoAction { label: qsTr("Add folder…"); iconName: "plus"; onClicked: root.addFolder() }
            }
            Repeater {
                model: photoLibrary.folders
                delegate: Item {
                    id: folderRow
                    required property var modelData
                    Layout.fillWidth: true; implicitHeight: 34
                    HoverHandler { id: rowHover }
                    LibraryRow {
                        id: folderLabel
                        anchors.left: parent.left; anchors.right: rowActions.left; height: parent.height
                        text: folderRow.modelData.name || folderRow.modelData.path; symbol: "folder"
                        highlighted: root.browserPage === "library" && photoLibrary.mode === "folder" && photoLibrary.folder.toString() === folderRow.modelData.url.toString()
                        onClicked: root.navigate("folder", folderRow.modelData.url)
                    }
                    PhotoAction {
                        id: rowActions
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        opacity: rowHover.hovered || activeFocus || folderLabel.activeFocus ? 1 : 0
                        label: qsTr("Folder actions"); iconName: "ellipsis"
                        onClicked: root.folderActions(folderRow.modelData, rowActions)
                    }
                }
            }
            Label {
                Layout.fillWidth: true; Layout.margins: 12
                visible: photoLibrary.folders.length === 0
                text: qsTr("Referenced folders stay in their original location.")
                color: AppTheme.mutedText; font.pixelSize: 11; wrapMode: Text.Wrap
            }
        }
    }
}
