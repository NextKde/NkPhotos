// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Kos.Ui

Item {
    id: root
    property url source
    property bool active: visible
    property bool autoPlay: false
    readonly property bool localSource: source.toString().startsWith("file:")
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property string errorString: active && !source.toString() ? qsTr("The video file is unavailable.")
        : source.toString() && !localSource ? qsTr("Only local video files can be played.") : player.errorString
    signal playbackFinished()
    signal playbackFailed(string message)
    signal playbackToggled(bool playing)

    function play() { if (active && localSource) player.play() }
    function pause() { player.pause() }
    function stop() { player.stop() }
    function togglePlayback() {
        const resume = !playing
        if (resume) play(); else pause()
        playbackToggled(resume)
    }
    function restartIfRequested() {
        player.stop()
        Qt.callLater(function() { if (root.active && root.autoPlay) root.play() })
    }
    onSourceChanged: restartIfRequested()
    onActiveChanged: restartIfRequested()
    onAutoPlayChanged: { if (autoPlay) play(); else pause() }
    Component.onDestruction: player.stop()

    MediaPlayer {
        id: player
        source: root.active && root.localSource ? root.source : ""
        audioOutput: AudioOutput { id: sound; volume: volumeSlider.value }
        videoOutput: video
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia) root.playbackFinished()
            else if (mediaStatus === MediaPlayer.LoadedMedia && root.autoPlay) root.play()
        }
        onErrorOccurred: (error, errorString) => root.playbackFailed(errorString)
    }
    VideoOutput {
        id: video
        anchors.fill: parent; anchors.bottomMargin: 52
        fillMode: VideoOutput.PreserveAspectFit
    }
    BusyIndicator {
        anchors.centerIn: video
        running: player.mediaStatus === MediaPlayer.LoadingMedia || player.mediaStatus === MediaPlayer.StalledMedia
        visible: running
    }
    ColumnLayout {
        anchors.centerIn: video; width: Math.max(0, Math.min(420, video.width - 40)); spacing: 10
        visible: !!root.errorString
        Label {
            Layout.fillWidth: true; text: qsTr("Video unavailable")
            horizontalAlignment: Text.AlignHCenter; color: AppTheme.text; font.pixelSize: 21
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
            text: qsTr("The file cannot be played. It may be inaccessible or use an unsupported codec.")
            color: AppTheme.mutedText
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
            text: root.errorString; textFormat: Text.PlainText; color: AppTheme.mutedText; font.pixelSize: 12
        }
    }
    RowLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 48; spacing: 8
        PhotoAction {
            label: root.playing ? qsTr("Pause video") : qsTr("Play video")
            iconName: root.playing ? "pause" : "play"
            enabled: root.active && root.localSource && !root.errorString
            onClicked: root.togglePlayback()
        }
        Label {
            text: root.timeText(seek.pressed ? seek.value : player.position)
            color: AppTheme.mutedText; font.pixelSize: 12
        }
        Slider {
            id: seek; Layout.fillWidth: true; from: 0; to: Math.max(1, player.duration)
            value: player.position; enabled: player.seekable
            Accessible.name: qsTr("Video position")
            onMoved: player.position = value
        }
        Label { text: root.timeText(player.duration); color: AppTheme.mutedText; font.pixelSize: 12 }
        PhotoAction {
            label: sound.muted ? qsTr("Unmute video") : qsTr("Mute video")
            iconName: sound.muted ? "speaker_slash" : "speaker_2"
            onClicked: sound.muted = !sound.muted
        }
        Slider {
            id: volumeSlider; Layout.preferredWidth: 80; from: 0; to: 1; value: 0.8
            Accessible.name: qsTr("Video volume")
        }
    }
    function timeText(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        const minutes = Math.floor(seconds / 60)
        return minutes + ":" + (seconds % 60).toString().padStart(2, "0")
    }
}
