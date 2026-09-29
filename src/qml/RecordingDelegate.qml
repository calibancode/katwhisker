// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.coreaddons as KCoreAddons

QQC2.ItemDelegate {
    id: delegate

    required property int index
    required property string title
    required property string artist
    required property string station
    required property string favicon
    required property double duration
    required property string fileName

    signal exportRequested()
    signal discardRequested()

    // Exporting is the one thing to do with a recording, so a click does it.
    onClicked: exportRequested()

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        StationIcon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: delegate.favicon
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: delegate.title
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                textFormat: Text.PlainText
                text: [delegate.artist, delegate.station, KCoreAddons.Format.formatDuration(delegate.duration, KCoreAddons.FormatTypes.FoldHours)]
                      .filter(s => s).join("  ·  ")
            }
        }

        IconToolButton {
            icon.name: "document-save-as"
            text: i18n("Export…")
            onClicked: delegate.exportRequested()
        }
        IconToolButton {
            icon.name: "edit-delete"
            text: i18n("Discard")
            onClicked: delegate.discardRequested()
        }
    }
}
