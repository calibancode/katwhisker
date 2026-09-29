pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: delegate

    required property var modelData
    readonly property var station: modelData
    property bool active: false
    property bool playing: false
    property bool favorite: false

    signal playRequested()
    signal favoriteToggled()
    signal tagClicked(string tag)
    signal voteRequested()
    signal copyRequested()

    highlighted: active
    onClicked: playRequested()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: menu.popup()
    }
    onPressAndHold: menu.popup()

    QQC2.Menu {
        id: menu
        QQC2.MenuItem {
            text: i18n("Play")
            icon.name: "media-playback-start"
            onTriggered: delegate.playRequested()
        }
        QQC2.MenuItem {
            text: delegate.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
            icon.name: delegate.favorite ? "starred-symbolic" : "non-starred-symbolic"
            onTriggered: delegate.favoriteToggled()
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18n("Open Homepage")
            icon.name: "internet-services"
            enabled: (delegate.station.homepage ?? "") !== ""
            onTriggered: Qt.openUrlExternally(delegate.station.homepage)
        }
        QQC2.MenuItem {
            text: i18n("Copy Stream URL")
            icon.name: "edit-copy"
            onTriggered: delegate.copyRequested()
        }
        QQC2.MenuItem {
            text: i18n("Vote for Station")
            icon.name: "thumbs-up"
            onTriggered: delegate.voteRequested()
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        StationIcon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: delegate.station.favicon

            Kirigami.Icon {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: -Kirigami.Units.smallSpacing
                width: Kirigami.Units.iconSizes.small
                height: width
                source: "media-playback-playing"
                visible: delegate.playing
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: delegate.station.name
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.bold: delegate.active
            }
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                textFormat: Text.PlainText
                text: [delegate.station.country,
                       delegate.station.bitrate > 0 ? i18n("%1 kbps", delegate.station.bitrate) : "",
                       delegate.station.codec,
                       delegate.station.votes > 0 ? i18np("%1 vote", "%1 votes", delegate.station.votes) : ""]
                      .filter(s => s).join("  ·  ")
            }
            // Tags are links: clicking one browses all stations with that tag.
            QQC2.Label {
                Layout.fillWidth: true
                visible: text !== ""
                elide: Text.ElideRight
                font: Kirigami.Theme.smallFont
                textFormat: Text.StyledText
                linkColor: Kirigami.Theme.linkColor
                text: (Array.isArray(delegate.station.tags) ? delegate.station.tags : [])
                      .map(t => `<a href="${encodeURIComponent(t)}">${t.replace(/[<>&"]/g, "")}</a>`)
                      .join("  ")
                onLinkActivated: link => delegate.tagClicked(decodeURIComponent(link))

                HoverHandler {
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }

        IconToolButton {
            icon.name: delegate.favorite ? "starred-symbolic" : "non-starred-symbolic"
            text: delegate.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
            onClicked: delegate.favoriteToggled()
        }
    }
}
