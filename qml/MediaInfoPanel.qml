// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kos.Ui

LiquidContentPopover {
    id: root
    property var photo: ({})
    property string editingId: ""
    preferredSide: "bottom"
    width: 330; height: Math.min(500, parent ? parent.height - 24 : 500)
    function load() {
        editingId = photo.mediaId || photo.id || ""
        titleField.text = photo.title || ""
        descriptionField.text = photo.description || ""
        keywordsField.text = Array.from(photo.keywords || []).join(", ")
    }
    onOpened: load()
    onPhotoChanged: if (visible && editingId !== (photo.mediaId || photo.id || "")) load()
    contentItem: ScrollView {
        clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 12
            Label { text: qsTr("Information"); color: AppTheme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            Label { Layout.fillWidth: true; text: root.photo.name || ""; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: AppTheme.text; font.pixelSize: 13 }
            Label { Layout.fillWidth: true; text: (root.photo.mediaType === "video" ? qsTr("Video") : qsTr("Photo")) + (root.photo.width > 0 ? " · " + root.photo.width + " × " + root.photo.height : "") + (root.photo.duration > 0 ? " · " + qsTr("%1 sec").arg(Math.round(root.photo.duration / 1000)) : ""); color: AppTheme.mutedText; wrapMode: Text.Wrap; font.pixelSize: 12 }
            Label { Layout.fillWidth: true; text: root.photo.date ? Qt.formatDateTime(new Date(root.photo.date), Qt.DefaultLocaleShortDate) : "—"; color: AppTheme.text; wrapMode: Text.Wrap; font.pixelSize: 12 }
            Label { Layout.fillWidth: true; text: root.photo.dateSource === "file" ? qsTr("File date — capture date unavailable") : qsTr("Capture date"); color: AppTheme.mutedText; wrapMode: Text.Wrap; font.pixelSize: 11 }
            Label { Layout.fillWidth: true; visible: !!root.photo.camera; text: root.photo.camera || ""; color: AppTheme.text; wrapMode: Text.Wrap; font.pixelSize: 12; textFormat: Text.PlainText }
            Label { Layout.fillWidth: true; text: root.photo.bytes === undefined ? "" : (root.photo.bytes / 1048576).toFixed(2) + " MB"; color: AppTheme.mutedText; font.pixelSize: 12 }
            Rectangle { Layout.fillWidth: true; height: 1; color: AppTheme.withAlpha(AppTheme.text,.1) }
            Label { text: qsTr("Title"); color: AppTheme.mutedText; font.pixelSize: 11 }
            LiquidTextField { id: titleField; objectName: "metadataTitle"; Layout.fillWidth: true; materialForm: true; Accessible.name: qsTr("Title") }
            Label { text: qsTr("Description"); color: AppTheme.mutedText; font.pixelSize: 11 }
            TextArea { id: descriptionField; objectName: "metadataDescription"; Layout.fillWidth: true; Layout.preferredHeight: 74; wrapMode: TextEdit.Wrap; selectByMouse: true; color: AppTheme.text; Accessible.name: qsTr("Description"); background: Rectangle { radius: 7; color: AppTheme.contentSecondaryBackground; border.color: AppTheme.withAlpha(AppTheme.text,.12) } }
            Label { text: qsTr("Keywords, separated by commas"); color: AppTheme.mutedText; font.pixelSize: 11 }
            LiquidTextField { id: keywordsField; objectName: "metadataKeywords"; Layout.fillWidth: true; materialForm: true; Accessible.name: qsTr("Keywords") }
            LiquidActionButton { Layout.alignment: Qt.AlignRight; text: qsTr("Save information"); toolbarAction: true; materialForm: true; enabled: !!root.editingId && !root.photo.trashed && !root.photo.temporary; onClicked: { photoLibrary.updateMetadata(root.editingId, titleField.text, descriptionField.text, keywordsField.text.split(",").map(s => s.trim()).filter(s => s.length > 0)); root.close() } }
            Label { Layout.fillWidth: true; text: root.photo.temporary ? qsTr("Import this item to save information in your library.") : qsTr("These details are saved in your library. Original files are not changed."); wrapMode: Text.Wrap; color: AppTheme.mutedText; font.pixelSize: 11 }
            Label { text: root.photo.managed ? qsTr("Imported copy") : qsTr("Referenced original"); color: AppTheme.text; font.pixelSize: 12 }
            Label { Layout.fillWidth: true; text: root.photo.path || ""; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: AppTheme.mutedText; font.pixelSize: 11 }
            Label { Layout.fillWidth: true; visible: root.photo.available === false; text: qsTr("This file is unavailable. Locate its folder again using the folder menu."); color: AppTheme.mutedText; wrapMode: Text.Wrap; font.pixelSize: 12 }
        }
    }
}
