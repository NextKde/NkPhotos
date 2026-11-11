// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

GridView {
    id: root
    objectName: "dateBrowser"
    clip: true
    model: photoLibrary.dateGroups
    readonly property int columns: Math.max(1, Math.floor(width / 260))
    cellWidth: width / columns; cellHeight: Math.min(280, cellWidth * .72) + 22
    signal openDate(string key)
    signal createMemory(string key, string label)
    delegate: Item {
        id: card
        required property var modelData
        width: root.cellWidth; height: root.cellHeight
        activeFocusOnTab: true
        Keys.onReturnPressed: root.openDate(modelData.key)
        Keys.onSpacePressed: root.openDate(modelData.key)
        Rectangle {
            id: frame
            anchors.fill: parent; anchors.margins: 7; radius: 12; clip: true
            color: AppTheme.contentSecondaryBackground
            Image { anchors.fill: parent; source: card.modelData.imageSource ? card.modelData.imageSource + "/thumb" : ""; sourceSize: Qt.size(640,480); asynchronous: true; fillMode: Image.PreserveAspectCrop }
            Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 1; color: "#b8000000" } } }
            MouseArea { anchors.fill: parent; onClicked: root.openDate(card.modelData.key); cursorShape: Qt.PointingHandCursor }
            Column {
                anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 18; anchors.right: memoryButton.left; spacing: 4
                Label { width: parent.width; text: card.modelData.label; color: "white"; font.pixelSize: 25; font.weight: Font.DemiBold; elide: Text.ElideRight; textFormat: Text.PlainText }
                Label { text: qsTr("%1 items").arg(card.modelData.count); color: "#e6ffffff"; font.pixelSize: 12 }
            }
            PhotoAction { id: memoryButton; visible: ["hidden","trash","session"].indexOf(photoLibrary.mode) < 0; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 12; label: qsTr("Create a memory from this date"); iconName: "plus"; onClicked: root.createMemory(card.modelData.key, card.modelData.label) }
        }
        Accessible.role: Accessible.Button
        Accessible.name: card.modelData.label + ", " + qsTr("%1 items").arg(card.modelData.count)
        Accessible.onPressAction: root.openDate(card.modelData.key)
    }
    ScrollBar.vertical: ScrollBar {}
}
