pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

QQC2.ToolBar {
    id: bar

    property var station: null
    property bool playing: false
    property bool connecting: false
    property bool favorite: false
    property bool canStep: false
    property string nowPlaying: ""
    property string errorString: ""
    property AudioOutput audioOutput

    signal toggleRequested()
    signal nextRequested()
    signal previousRequested()
    signal favoriteToggled()
    signal copyTitleRequested()

    position: QQC2.ToolBar.Footer

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        StationIcon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.large
            Layout.preferredHeight: Kirigami.Units.iconSizes.large
            source: bar.station?.favicon ?? ""
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: bar.nowPlaying || (bar.station?.name ?? "")
                elide: Text.ElideRight
                font.bold: true
                textFormat: Text.PlainText

                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: titleArea.containsMouse && truncated
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                MouseArea {
                    id: titleArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.RightButton
                    onClicked: if (bar.nowPlaying) titleMenu.popup()
                }
                QQC2.Menu {
                    id: titleMenu
                    QQC2.MenuItem {
                        text: i18n("Copy Song Title")
                        icon.name: "edit-copy"
                        onTriggered: bar.copyTitleRequested()
                    }
                }
            }
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: bar.errorString ? 1 : 0.7
                color: bar.errorString ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                font: Kirigami.Theme.smallFont
                textFormat: Text.PlainText
                text: bar.errorString ? bar.errorString
                    : bar.connecting ? i18n("Connecting…")
                    : bar.nowPlaying ? (bar.station?.name ?? "")
                    : bar.playing ? i18n("Live") : i18n("Stopped")
            }
        }

        QQC2.ToolButton {
            icon.name: bar.favorite ? "starred-symbolic" : "non-starred-symbolic"
            text: bar.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
            display: QQC2.AbstractButton.IconOnly
            onClicked: bar.favoriteToggled()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            visible: bar.canStep
            icon.name: "media-skip-backward"
            text: i18n("Previous Station")
            display: QQC2.AbstractButton.IconOnly
            onClicked: bar.previousRequested()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            icon.name: bar.playing ? "media-playback-stop" : "media-playback-start"
            text: bar.playing ? i18n("Stop") : i18n("Play")
            display: QQC2.AbstractButton.IconOnly
            onClicked: bar.toggleRequested()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            QQC2.BusyIndicator {
                anchors.fill: parent
                running: bar.connecting
                visible: running
            }
        }

        QQC2.ToolButton {
            visible: bar.canStep
            icon.name: "media-skip-forward"
            text: i18n("Next Station")
            display: QQC2.AbstractButton.IconOnly
            onClicked: bar.nextRequested()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.ToolButton {
            id: volumeButton
            readonly property real level: bar.audioOutput.muted ? 0 : bar.audioOutput.volume
            icon.name: level === 0 ? "audio-volume-muted"
                     : level < 0.34 ? "audio-volume-low"
                     : level < 0.67 ? "audio-volume-medium" : "audio-volume-high"
            text: i18n("Volume")
            display: QQC2.AbstractButton.IconOnly
            onClicked: volumePopup.opened ? volumePopup.close() : volumePopup.open()
            QQC2.ToolTip.text: i18n("Volume: %1% (middle-click to mute)", Math.round(level * 100))
            QQC2.ToolTip.visible: hovered && !volumePopup.opened
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            // Scroll to change volume and middle-click to mute, like Plasma's applets.
            WheelHandler {
                onWheel: event => {
                    bar.audioOutput.muted = false
                    bar.audioOutput.volume = Math.max(0, Math.min(1, bar.audioOutput.volume + event.angleDelta.y / 120 * 0.05))
                }
            }
            TapHandler {
                acceptedButtons: Qt.MiddleButton
                onTapped: bar.audioOutput.muted = !bar.audioOutput.muted
            }

            QQC2.Popup {
                id: volumePopup
                y: -height - Kirigami.Units.smallSpacing
                x: (parent.width - width) / 2
                padding: Kirigami.Units.smallSpacing
                contentItem: QQC2.Slider {
                    orientation: Qt.Vertical
                    implicitHeight: Kirigami.Units.gridUnit * 7
                    from: 0; to: 1
                    value: volumeButton.level
                    onMoved: { bar.audioOutput.volume = value; bar.audioOutput.muted = false }
                }
            }
        }
    }
}
