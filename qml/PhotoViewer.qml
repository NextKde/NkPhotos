// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Kos.Ui

FocusScope {
    id: root
    required property var photo
    property int position: -1
    property int total: 0
    property bool slideshowEnabled: false
    property bool playing: false
    property real intervalSeconds: 5
    property url musicUrl
    property bool musicMuted: false
    property bool loop: false
    readonly property bool isVideo: photo && photo.mediaType === "video"
    readonly property string musicError: musicUrl.toString() && !musicUrl.toString().startsWith("file:")
        ? qsTr("Choose a local music file for the slideshow.") : music.errorString
    signal previous()
    signal next()
    signal advance()
    signal finished()
    Shortcut {
        sequence: "Space"
        enabled: root.visible && root.activeFocus
        onActivated: root.togglePlayback()
    }
    function togglePlayback() {
        if (slideshowEnabled) playing = !playing
        else if (isVideo) video.togglePlayback()
        else next()
    }
    function stopSlideshow() { playing = false; slideshowEnabled = false; music.stop(); video.stop() }
    function advanceSlideshow() {
        if (!visible || !slideshowEnabled || !playing) return
        if (position >= 0 && (position + 1 < total || loop)) {
            advance()
            // A one-item looping collection keeps the same source URL.
            if (loop && total === 1 && isVideo) video.play()
        } else { playing = false; finished() }
    }
    onVisibleChanged: if (!visible) stopSlideshow()
    onSlideshowEnabledChanged: if (!slideshowEnabled) { playing = false; music.stop() }
    onPlayingChanged: {
        if (playing && !slideshowEnabled) slideshowEnabled = true
        if (playing && visible && music.source.toString()) music.play()
        else music.stop()
    }
    onPhotoChanged: slideTimer.restartIfReady()
    Component.onDestruction: music.stop()
    Timer {
        id: slideTimer
        interval: Math.max(1, root.intervalSeconds) * 1000
        repeat: true
        running: root.visible && root.slideshowEnabled && root.playing && !root.isVideo && picture.status === Image.Ready
        function restartIfReady() { if (running) restart() }
        onTriggered: root.advanceSlideshow()
    }
    MediaPlayer {
        id: music
        source: root.visible && root.slideshowEnabled && root.musicUrl.toString().startsWith("file:") ? root.musicUrl : ""
        loops: MediaPlayer.Infinite
        audioOutput: AudioOutput { muted: root.musicMuted || root.isVideo }
        onSourceChanged: {
            stop()
            if (root.playing && source.toString()) play()
        }
        onMediaStatusChanged: if (mediaStatus === MediaPlayer.LoadedMedia && root.playing) play()
    }
    function zoom(factor) {
        if (isVideo || picture.status !== Image.Ready || area.contentWidth <= 0 || area.contentHeight <= 0) return
        // Keep the image point beneath the viewport centre stable after panning.
        const x = (area.width/2-area.contentX)/area.contentWidth
        const y = (area.height/2-area.contentY)/area.contentHeight
        area.leaveAutoFit()
        const size = area.multiplyContentSize(factor)
        area.contentWidth = size.width; area.contentHeight = size.height
        area.contentX = area.boundedContentX(area.width/2-x*size.width,size.width)
        area.contentY = area.boundedContentY(area.height/2-y*size.height,size.height)
    }
    function fit() { area.resetToDefaultSize() }
    Rectangle { anchors.fill: parent; color: AppTheme.dark ? "#111113" : "#f7f7f7" }
    ZoomArea {
        id: area; objectName: "zoomArea"
        visible: !root.isVideo
        anchors.fill: parent; anchors.margins: 20; anchors.bottomMargin: root.slideshowEnabled ? 108 : 64
        implicitContentWidth: picture.implicitWidth
        implicitContentHeight: picture.implicitHeight
        maximumZoomFactor: 16
        Image {
            id: picture; objectName: "fullImage"
            anchors.fill: parent
            source: root.visible && !root.isVideo && root.photo && root.photo.imageSource ? root.photo.imageSource + "/full" : ""
            sourceSize: Qt.size(4096,4096)
            asynchronous: true; cache: false; retainWhileLoading: false
            fillMode: Image.PreserveAspectFit
            onSourceChanged: area.resetToDefaultSize()
            onStatusChanged: {
                if (status === Image.Ready) area.resetToDefaultSize()
                else if (status === Image.Error && root.slideshowEnabled) root.playing = false
            }
        }
    }
    VideoViewer {
        id: video
        anchors.fill: parent; anchors.margins: 20; anchors.bottomMargin: root.slideshowEnabled ? 108 : 64
        visible: root.isVideo
        active: root.visible && root.isVideo
        source: root.isVideo ? root.photo.url || "" : ""
        autoPlay: root.slideshowEnabled && root.playing
        onPlaybackFinished: root.advanceSlideshow()
        onPlaybackFailed: if (root.slideshowEnabled) root.playing = false
        onPlaybackToggled: playing => { if (root.slideshowEnabled) root.playing = playing }
    }
    ColumnLayout {
        anchors.centerIn: area; width: Math.min(380,area.width-40); spacing: 12
        visible: !root.isVideo && (picture.status === Image.Error || !root.photo || !root.photo.imageSource)
        VectorIcon { Layout.alignment: Qt.AlignHCenter; name: "photo"; size: 36; color: AppTheme.mutedText }
        Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTr("Image unavailable"); color: AppTheme.text; font.pixelSize: 21 }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; text: qsTr("The file may have moved, be unsupported, or exceed the safe preview limit. Reopen it or refresh the folder."); color: AppTheme.mutedText; font.pixelSize: 13 }
    }
    BusyIndicator { anchors.centerIn: area; running: !root.isVideo && picture.status === Image.Loading; visible: running }
    SlideshowControls {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.bottomMargin: 44
        height: 44; visible: root.slideshowEnabled
        playing: root.playing; intervalSeconds: root.intervalSeconds; muted: root.musicMuted
        hasMusic: !!root.musicUrl.toString(); errorMessage: root.musicError
        onTogglePlayback: root.togglePlayback()
        onIntervalEdited: seconds => root.intervalSeconds = seconds
        onToggleMuted: root.musicMuted = !root.musicMuted
        onStop: { root.stopSlideshow(); root.finished() }
    }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 44; color: AppTheme.contentBackground
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 6
            PhotoAction { objectName: "previousButton"; label: qsTr("Previous image"); iconName: "chevron_left"; enabled: root.position > 0; onClicked: root.previous() }
            PhotoAction { objectName: "nextButton"; label: qsTr("Next image"); iconName: "chevron_right"; enabled: root.position >= 0 && root.position+1 < root.total; onClicked: root.next() }
            Item { Layout.fillWidth: true }
            PhotoAction { objectName: "zoomOutButton"; visible: !root.isVideo; label: qsTr("Zoom out"); iconName: "minus"; enabled: picture.status === Image.Ready; onClicked: root.zoom(.8) }
            LiquidActionButton {
                id: fitButton; objectName: "fitButton"
                visible: !root.isVideo
                text: area.autoFit ? qsTr("Fit") : Math.round(area.zoomFactor*100)+"%"
                toolbarAction: true; materialForm: true; flat: true
                enabled: picture.status === Image.Ready
                Accessible.name: qsTr("Fit image to window")
                onClicked: root.fit()
                KosToolTip { target: fitButton; text: qsTr("Fit image to window. Zoom is relative to the preview.") }
            }
            PhotoAction { objectName: "zoomInButton"; visible: !root.isVideo; label: qsTr("Zoom in"); iconName: "plus"; enabled: picture.status === Image.Ready; onClicked: root.zoom(1.25) }
        }
    }
}
