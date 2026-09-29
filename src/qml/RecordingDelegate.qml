pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

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

    function formatDuration(ms) {
        const s = Math.round(ms / 1000);
        return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
    }

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
                text: [delegate.artist, delegate.station, delegate.formatDuration(delegate.duration)]
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
