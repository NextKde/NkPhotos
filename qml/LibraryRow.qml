// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

KosItemDelegate {
    id: root
    property string symbol: "folder"
    implicitHeight: 34
    palette.highlightedText: AppTheme.text
    Accessible.name: text
    contentItem: RowLayout {
        spacing: 10
        VectorIcon { name: root.symbol; size: 16; color: AppTheme.mutedText }
        Label {
            id: caption
            Layout.fillWidth: true
            text: root.text; textFormat: Text.PlainText; elide: Text.ElideRight
            color: AppTheme.text; font.pixelSize: 13
            font.weight: root.highlighted ? Font.Medium : Font.Normal
        }
    }
    KosToolTip { target: root; text: root.text; onlyWhenTruncated: true; truncated: caption.truncated }
}
