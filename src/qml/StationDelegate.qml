import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: delegate

    required property var modelData
    readonly property var station: modelData
    property bool active: false
    property bool favorite: false

    signal playRequested()
    signal favoriteToggled()

    highlighted: active
    onClicked: playRequested()

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Item {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium

            Image {
                id: favicon
                anchors.fill: parent
                source: delegate.station.favicon
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Kirigami.Icon {
                anchors.fill: parent
                source: "radio"
                visible: favicon.status !== Image.Ready
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            QQC2.Label {
                Layout.fillWidth: true
                text: delegate.station.name
                elide: Text.ElideRight
                font.bold: delegate.active
            }
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                text: [delegate.station.country,
                       delegate.station.bitrate > 0 ? i18n("%1 kbps", delegate.station.bitrate) : "",
                       delegate.station.codec,
                       delegate.station.tags].filter(s => s).join("  ·  ")
            }
        }

        QQC2.ToolButton {
            icon.name: delegate.favorite ? "starred-symbolic" : "non-starred-symbolic"
            text: delegate.favorite ? i18n("Remove from favorites") : i18n("Add to favorites")
            display: QQC2.AbstractButton.IconOnly
            onClicked: delegate.favoriteToggled()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
        }
    }
}
