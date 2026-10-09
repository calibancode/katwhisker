// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A song heard earlier, see root.history in Main.qml.
QQC2.ItemDelegate {
    id: delegate

    required property var modelData
    readonly property var song: modelData

    signal copyRequested()
    signal removeRequested()

    // Copying the title (to look it up elsewhere) is the one thing to do
    // with a song, so a click does it.
    onClicked: copyRequested()

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        StationIcon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: delegate.song.favicon ?? ""
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: delegate.song.title
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                textFormat: Text.PlainText
                text: [delegate.song.artist, delegate.song.station, new Date(delegate.song.time).toLocaleString(Qt.locale(), Locale.ShortFormat)]
                      .filter(s => s).join("  ·  ")
            }
        }

        IconToolButton {
            icon.name: "edit-copy"
            text: i18n("Copy Song Title")
            onClicked: delegate.copyRequested()
        }
        IconToolButton {
            icon.name: "edit-delete"
            text: i18n("Remove")
            onClicked: delegate.removeRequested()
        }
    }
}
