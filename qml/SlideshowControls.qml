// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

Rectangle {
    id: root
    property bool playing: false
    property real intervalSeconds: 5
    property bool muted: false
    property bool hasMusic: false
    property string errorMessage
    signal togglePlayback()
    signal intervalEdited(real seconds)
    signal toggleMuted()
    signal stop()
    color: AppTheme.contentBackground
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 8
        PhotoAction {
            label: root.playing ? qsTr("Pause slideshow") : qsTr("Play slideshow")
            iconName: root.playing ? "pause" : "play"
            onClicked: root.togglePlayback()
        }
        Label { text: qsTr("Photo duration"); color: AppTheme.mutedText; font.pixelSize: 12 }
        SpinBox {
            from: 1; to: 60; value: Math.round(root.intervalSeconds); editable: true
            Accessible.name: qsTr("Seconds per photo")
            onValueModified: root.intervalEdited(value)
        }
        Label { text: qsTr("s"); color: AppTheme.mutedText }
        Item { Layout.fillWidth: true }
        Label {
            Layout.maximumWidth: 220; Layout.fillWidth: !!root.errorMessage
            visible: !!root.errorMessage; text: root.errorMessage; textFormat: Text.PlainText
            elide: Text.ElideRight; color: AppTheme.mutedText; font.pixelSize: 12
            KosToolTip { target: parent; text: root.errorMessage }
        }
        PhotoAction {
            visible: root.hasMusic
            label: root.muted ? qsTr("Unmute slideshow music") : qsTr("Mute slideshow music")
            iconName: root.muted ? "speaker_slash" : "speaker_2"
            onClicked: root.toggleMuted()
        }
        PhotoAction { label: qsTr("End slideshow"); iconName: "xmark"; onClicked: root.stop() }
    }
}
