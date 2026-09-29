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
    property string recordingTitle: ""
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
                id: titleLabel

                readonly property bool recording: bar.recordingTitle !== ""
                readonly property real dotSize: Kirigami.Units.iconSizes.small
                readonly property real gap: Kirigami.Units.smallSpacing
                // The dot follows the text until it would run off the edge,
                // then stays pinned there with the text fading out beneath it.
                readonly property bool pinned: recording && implicitWidth + gap + dotSize > width
                readonly property bool rtl: effectiveHorizontalAlignment === Text.AlignRight

                Layout.fillWidth: true
                text: bar.nowPlaying || (bar.station?.name ?? "")
                // The fade replaces the ellipsis while the dot is pinned.
                elide: pinned ? Text.ElideNone : Text.ElideRight
                clip: pinned
                font.bold: true
                textFormat: Text.PlainText

                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: titleArea.containsMouse && (truncated || pinned)
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                Rectangle {
                    id: fade
                    // A cheap fade in the bar's own background color, so it
                    // follows light/dark and custom color schemes.
                    readonly property color base: Kirigami.Theme.backgroundColor
                    visible: titleLabel.pinned
                    width: Kirigami.Units.gridUnit * 2 + titleLabel.dotSize + titleLabel.gap
                    height: parent.height
                    x: titleLabel.rtl ? 0 : parent.width - width
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: Qt.alpha(fade.base, titleLabel.rtl ? 1 : 0) }
                        GradientStop { position: titleLabel.rtl ? 0.35 : 0.65; color: fade.base }
                        GradientStop { position: 1; color: Qt.alpha(fade.base, titleLabel.rtl ? 0 : 1) }
                    }
                }

                Kirigami.Icon {
                    id: recordingDot
                    visible: titleLabel.recording
                    width: titleLabel.dotSize
                    height: titleLabel.dotSize
                    anchors.verticalCenter: parent.verticalCenter
                    x: {
                        const natural = titleLabel.implicitWidth + titleLabel.gap;
                        if (titleLabel.rtl)
                            return titleLabel.pinned ? 0 : titleLabel.width - natural - width;
                        return titleLabel.pinned ? titleLabel.width - width : natural;
                    }
                    source: "media-record"
                    color: Kirigami.Theme.negativeTextColor
                    isMask: true

                    Accessible.name: i18n("Recording “%1”", bar.recordingTitle)
                    HoverHandler { id: recordingHover }
                    QQC2.ToolTip.text: i18n("Recording “%1”", bar.recordingTitle)
                    QQC2.ToolTip.visible: recordingHover.hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }

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

        IconToolButton {
            icon.name: bar.favorite ? "starred-symbolic" : "non-starred-symbolic"
            text: bar.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
            onClicked: bar.favoriteToggled()
        }

        IconToolButton {
            visible: bar.canStep
            icon.name: "media-skip-backward"
            text: i18n("Previous Station")
            onClicked: bar.previousRequested()
        }

        IconToolButton {
            icon.name: bar.playing ? "media-playback-stop" : "media-playback-start"
            text: bar.playing ? i18n("Stop") : i18n("Play")
            onClicked: bar.toggleRequested()

            QQC2.BusyIndicator {
                anchors.fill: parent
                running: bar.connecting
                visible: running
            }
        }

        IconToolButton {
            visible: bar.canStep
            icon.name: "media-skip-forward"
            text: i18n("Next Station")
            onClicked: bar.nextRequested()
        }

        IconToolButton {
            id: volumeButton
            readonly property real level: bar.audioOutput.muted ? 0 : bar.audioOutput.volume
            icon.name: level === 0 ? "audio-volume-muted"
                     : level < 0.34 ? "audio-volume-low"
                     : level < 0.67 ? "audio-volume-medium" : "audio-volume-high"
            text: level === 0 ? i18n("Unmute") : i18n("Mute")
            onClicked: bar.audioOutput.muted = !bar.audioOutput.muted
            QQC2.ToolTip.text: i18n("Volume: %1%", Math.round(level * 100))

            // Scroll to change volume, like Plasma's applets.
            WheelHandler {
                onWheel: event => {
                    bar.audioOutput.muted = false
                    bar.audioOutput.volume = Math.max(0, Math.min(1, bar.audioOutput.volume + event.angleDelta.y / 120 * 0.05))
                }
            }
        }

        QQC2.Slider {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 5
            // Hide on narrow windows; the button still mutes and scrolls.
            visible: bar.width > Kirigami.Units.gridUnit * 22
            from: 0; to: 1
            value: volumeButton.level
            onMoved: { bar.audioOutput.volume = value; bar.audioOutput.muted = false }
        }
    }
}
