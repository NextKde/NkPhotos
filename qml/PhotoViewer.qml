// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

FocusScope {
    id: root
    required property var photo
    property bool showInfo: false
    property int position: -1
    property int total: 0
    signal previous()
    signal next()
    Keys.onLeftPressed: previous()
    Keys.onRightPressed: next()
    Keys.onSpacePressed: next()
    function zoom(factor) {
        if (picture.status !== Image.Ready) return
        area.leaveAutoFit()
        const size = area.multiplyContentSize(factor)
        area.contentWidth = size.width; area.contentHeight = size.height
        area.contentX = area.centerContentX(); area.contentY = area.centerContentY()
    }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            Rectangle { anchors.fill: parent; color: AppTheme.dark ? "#101114" : "#eef0f3" }
            ZoomArea {
                id: area
                objectName: "zoomArea"
                anchors.fill: parent; anchors.bottomMargin: 64; anchors.margins: 16
                implicitContentWidth: picture.implicitWidth
                implicitContentHeight: picture.implicitHeight
                maximumZoomFactor: 16
                Image {
                    id: picture
                    objectName: "fullImage"
                    anchors.fill: parent
                    source: root.visible && root.photo.imageSource ? root.photo.imageSource + "/full" : ""
                    sourceSize: Qt.size(4096,4096)
                    asynchronous: true; cache: false
                    fillMode: Image.PreserveAspectFit
                    onStatusChanged: if (status === Image.Ready) area.resetToDefaultSize()
                }
            }
            ColumnLayout {
                anchors.centerIn: parent; width: Math.min(380,parent.width - 40); spacing: 12
                visible: picture.status === Image.Error || !root.photo.imageSource
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Image unavailable"); color: AppTheme.text; font.pixelSize: 22 }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                    text: qsTr("The file may have moved, be unsupported, or exceed the safe preview limit. Reopen it or refresh the folder.")
                    color: AppTheme.mutedText
                }
            }
            BusyIndicator { anchors.centerIn: parent; running: picture.status === Image.Loading; visible: running }
            RowLayout {
                anchors.bottom: parent.bottom; anchors.bottomMargin: 16
                anchors.horizontalCenter: parent.horizontalCenter; spacing: 8
                LiquidActionButton { text: "‹"; Accessible.name: qsTr("Previous image"); materialForm: true; enabled: root.position > 0; onClicked: root.previous() }
                Label { text: root.position >= 0 ? (root.position + 1) + " / " + root.total : "—"; color: AppTheme.mutedText; Layout.minimumWidth: 58; horizontalAlignment: Text.AlignHCenter }
                LiquidActionButton { text: "›"; Accessible.name: qsTr("Next image"); materialForm: true; enabled: root.position >= 0 && root.position + 1 < root.total; onClicked: root.next() }
                LiquidActionButton { text: "−"; Accessible.name: qsTr("Zoom out"); materialForm: true; onClicked: root.zoom(.8) }
                LiquidActionButton { text: qsTr("Fit"); materialForm: true; onClicked: area.resetToDefaultSize() }
                LiquidActionButton { text: "+"; Accessible.name: qsTr("Zoom in"); materialForm: true; onClicked: root.zoom(1.25) }
            }
        }
        Rectangle {
            Layout.preferredWidth: 228; Layout.fillHeight: true; visible: root.showInfo
            color: AppTheme.contentSecondaryBackground
            ScrollView {
                anchors.fill: parent; anchors.margins: 20; clip: true
                contentWidth: availableWidth
                ColumnLayout {
                width: parent.width; spacing: 14
                Label { text: qsTr("Image information"); color: AppTheme.text; font.weight: Font.DemiBold; font.pixelSize: 15 }
                Label { Layout.fillWidth: true; text: root.photo.name || ""; color: AppTheme.text; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText }
                Label { text: qsTr("File size"); color: AppTheme.mutedText; font.pixelSize: 11 }
                Label { text: root.photo.bytes === undefined ? "—" : (root.photo.bytes / 1048576).toFixed(2) + " MB"; color: AppTheme.text }
                Label { text: qsTr("Modified"); color: AppTheme.mutedText; font.pixelSize: 11 }
                Label { Layout.fillWidth: true; text: root.photo.modified ? Qt.formatDateTime(root.photo.modified, Qt.DefaultLocaleShortDate) : "—"; color: AppTheme.text; wrapMode: Text.Wrap }
                Label { text: qsTr("Location"); color: AppTheme.mutedText; font.pixelSize: 11 }
                Label { Layout.fillWidth: true; text: root.photo.path || ""; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: AppTheme.text; font.pixelSize: 11 }
                Label { Layout.fillWidth: true; text: qsTr("Large images use a reduced preview. Editing in NkShot creates a separate copy."); wrapMode: Text.Wrap; color: AppTheme.mutedText; font.pixelSize: 11 }
                }
            }
        }
    }
}
