// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

Item {
    id: root
    property bool memories: false
    signal openCollection(var collection)
    signal collectionActions(var collection, Item anchor)
    signal createCollection()
    signal playMemory(var memory)
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 22; spacing: 16
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Label { text: root.memories ? qsTr("Memories") : qsTr("My albums"); color: AppTheme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
                Label { Layout.fillWidth: true; text: root.memories ? qsTr("Your moments, in your own order.") : qsTr("A place for every story."); color: AppTheme.mutedText; font.pixelSize: 12; wrapMode: Text.Wrap }
            }
            LiquidActionButton { text: root.memories ? qsTr("New memory…") : qsTr("New album…"); toolbarAction: true; materialForm: true; onClicked: root.createCollection() }
        }
        GridView {
            id: grid
            objectName: "collectionGrid"
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            model: root.memories ? photoLibrary.memories : photoLibrary.albums
            readonly property int columns: Math.max(1, Math.floor(width / (root.memories ? 240 : 190)))
            cellWidth: width / columns; cellHeight: root.memories ? cellWidth * .72 + 66 : cellWidth + 50
            delegate: Item {
                id: card
                required property var modelData
                width: grid.cellWidth; height: grid.cellHeight
                activeFocusOnTab: true
                Keys.onReturnPressed: root.openCollection(modelData)
                Keys.onSpacePressed: root.memories ? root.playMemory(modelData) : root.openCollection(modelData)
                Rectangle {
                    id: cover
                    x: 6; y: 6; width: parent.width - 12; height: root.memories ? width * .72 : width
                    radius: root.memories ? 14 : 8; color: AppTheme.contentSecondaryBackground; clip: true
                    Image { anchors.fill: parent; source: card.modelData.imageSource ? card.modelData.imageSource + "/thumb" : ""; asynchronous: true; sourceSize: Qt.size(512,512); fillMode: Image.PreserveAspectCrop }
                    VectorIcon { anchors.centerIn: parent; visible: !card.modelData.imageSource; name: root.memories ? "play_circle" : "photo_on_rectangle"; size: 36; color: AppTheme.mutedText }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.openCollection(card.modelData) }
                    Rectangle {
                        visible: root.memories && card.modelData.count > 0; width: 44; height: 44; radius: 22; color: "#a0000000"
                        anchors.centerIn: parent
                        VectorIcon { anchors.centerIn: parent; name: "play_fill"; size: 22; color: "white" }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.playMemory(card.modelData) }
                        Accessible.role: Accessible.Button; Accessible.name: qsTr("Play %1").arg(card.modelData.name)
                        Accessible.onPressAction: root.playMemory(card.modelData)
                    }
                }
                Column {
                    anchors.top: cover.bottom; anchors.topMargin: 10; anchors.left: cover.left; anchors.right: actions.left; spacing: 4
                    Label { width: parent.width; text: card.modelData.name; textFormat: Text.PlainText; elide: Text.ElideRight; color: AppTheme.text; font.pixelSize: 14; font.weight: Font.Medium }
                    Label { text: qsTr("%1 items").arg(card.modelData.count); color: AppTheme.mutedText; font.pixelSize: 12 }
                }
                PhotoAction { id: actions; anchors.right: cover.right; anchors.top: cover.bottom; anchors.topMargin: 5; label: qsTr("Collection actions"); iconName: "ellipsis"; onClicked: root.collectionActions(card.modelData, actions) }
                Accessible.role: Accessible.Button; Accessible.name: card.modelData.name
                Accessible.onPressAction: root.openCollection(card.modelData)
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
    ColumnLayout {
        visible: grid.count === 0
        anchors.centerIn: parent; width: Math.min(340, parent.width - 48); spacing: 14
        VectorIcon { Layout.alignment: Qt.AlignHCenter; name: root.memories ? "play_circle" : "rectangle_stack"; size: 46; color: AppTheme.mutedText }
        Label { Layout.fillWidth: true; text: root.memories ? qsTr("Make a memory") : qsTr("Start an album"); horizontalAlignment: Text.AlignHCenter; font.pixelSize: 23; font.weight: Font.Medium; color: AppTheme.text }
        Label { Layout.fillWidth: true; text: root.memories ? qsTr("Choose photos, an album or a date to build a personal slideshow with music.") : qsTr("Collect your favorite moments without making another copy of your files."); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: AppTheme.mutedText; font.pixelSize: 13 }
    }
}
