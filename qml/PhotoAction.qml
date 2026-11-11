// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import Kos.Ui

// Application semantics only; geometry, material and feedback remain canonical.
LiquidActionButton {
    id: root
    required property string label
    toolbarAction: true
    materialForm: true
    iconOnly: true
    flat: true
    Accessible.name: label
    KosToolTip { target: root; text: root.label }
}
