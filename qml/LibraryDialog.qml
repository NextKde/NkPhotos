// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import Kos.Ui

KosDialog {
    id: root
    parent: Overlay.overlay
    modal: true; focus: true
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - 32 : 460)
    height: Math.min(implicitHeight, parent ? parent.height - 24 : implicitHeight)
    padding: 22
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { radius: 16; color: AppTheme.contentBackground; border.width: 1; border.color: AppTheme.withAlpha(AppTheme.text,.12) }
    header: Label { text: root.title; color: AppTheme.text; font.pixelSize: 19; font.weight: Font.DemiBold; wrapMode: Text.Wrap; leftPadding: 22; rightPadding: 22; topPadding: 22; bottomPadding: 8 }
}
