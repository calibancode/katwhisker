import QtCore
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami
import org.kde.kradio

Kirigami.ApplicationWindow {
    id: root

    title: current ? current.name : i18n("KRadio")
    width: Kirigami.Units.gridUnit * 24
    height: Kirigami.Units.gridUnit * 34
    minimumWidth: Kirigami.Units.gridUnit * 16
    minimumHeight: Kirigami.Units.gridUnit * 16

    // Currently selected station (a plain JS object, see RadioBrowser::search).
    property var current: null
    property var favorites: []
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState

    function isFavorite(uuid) {
        return favorites.some(s => s.uuid === uuid)
    }
    function toggleFavorite(station) {
        favorites = isFavorite(station.uuid)
            ? favorites.filter(s => s.uuid !== station.uuid)
            : favorites.concat([station])
    }
    function play(station) {
        if (current && current.uuid === station.uuid && playing)
            return
        current = station
        player.source = station.url
        player.play()
        RadioBrowser.countClick(station.uuid)
    }
    function togglePlayback() {
        if (!current)
            return
        if (playing) {
            player.stop() // live streams: stop instead of buffering a pause
        } else {
            player.source = current.url
            player.play()
        }
    }

    // Everything the user touches is persisted automatically.
    Settings {
        id: settings
        property alias x: root.x
        property alias y: root.y
        property alias width: root.width
        property alias height: root.height
        property alias volume: audio.volume
        property alias muted: audio.muted
        property alias query: page.query
        property alias showFavorites: page.showFavorites
        property string favoritesJson: "[]"
        property string currentJson: ""
        property bool wasPlaying: false
    }

    Component.onCompleted: {
        try { favorites = JSON.parse(settings.favoritesJson) } catch (e) { favorites = [] }
        try { current = settings.currentJson ? JSON.parse(settings.currentJson) : null } catch (e) {}
        if (current && settings.wasPlaying) {
            player.source = current.url
            player.play()
        }
        RadioBrowser.search(page.query)
    }
    onFavoritesChanged: settings.favoritesJson = JSON.stringify(favorites)
    onCurrentChanged: settings.currentJson = current ? JSON.stringify(current) : ""
    onClosing: settings.wasPlaying = playing

    MediaPlayer {
        id: player
        audioOutput: AudioOutput { id: audio; volume: 0.8 }
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: page

        property string query: ""
        property bool showFavorites: false
        readonly property var model: showFavorites ? root.favorites : RadioBrowser.stations

        title: showFavorites ? i18n("Favorites") : i18n("Stations")

        titleDelegate: Kirigami.SearchField {
            Layout.fillWidth: true
            text: page.query
            visible: !page.showFavorites
            delaySearch: true
            onAccepted: {
                page.query = text
                RadioBrowser.search(text)
            }
        }

        actions: [
            Kirigami.Action {
                icon.name: "view-refresh"
                text: i18n("Refresh")
                displayHint: Kirigami.DisplayHint.IconOnly
                visible: !page.showFavorites
                onTriggered: RadioBrowser.search(page.query)
            },
            Kirigami.Action {
                icon.name: "starred-symbolic"
                text: i18n("Favorites")
                displayHint: Kirigami.DisplayHint.IconOnly
                checkable: true
                checked: page.showFavorites
                onToggled: page.showFavorites = checked
            }
        ]

        supportsRefreshing: !showFavorites
        refreshing: RadioBrowser.busy
        onRefreshingChanged: if (refreshing && !RadioBrowser.busy) RadioBrowser.search(query)

        ListView {
            id: list
            model: page.model
            delegate: StationDelegate {
                width: ListView.view.width
                station: modelData
                active: root.current !== null && root.current.uuid === modelData.uuid
                favorite: root.isFavorite(modelData.uuid)
                onPlayRequested: root.play(modelData)
                onFavoriteToggled: root.toggleFavorite(modelData)
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: list.count === 0 && !RadioBrowser.busy
                icon.name: RadioBrowser.error ? "network-disconnect"
                         : page.showFavorites ? "starred-symbolic" : "radio"
                text: RadioBrowser.error && !page.showFavorites ? i18n("Couldn't reach radio-browser.info")
                    : page.showFavorites ? i18n("No favorites yet")
                    : i18n("No stations found")
                explanation: RadioBrowser.error && !page.showFavorites ? RadioBrowser.error
                           : page.showFavorites ? i18n("Star a station to keep it here.") : ""
            }
        }

        footer: QQC2.ToolBar {
            visible: root.current !== null
            position: QQC2.ToolBar.Footer

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Image {
                    id: playerIcon
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    source: root.current ? root.current.favicon : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    visible: status === Image.Ready
                }
                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    source: "radio"
                    visible: !playerIcon.visible
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: root.current ? root.current.name : ""
                        elide: Text.ElideRight
                        font.bold: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        opacity: 0.7
                        font: Kirigami.Theme.smallFont
                        text: {
                            if (player.error !== MediaPlayer.NoError)
                                return player.errorString
                            if (player.mediaStatus === MediaPlayer.LoadingMedia
                                    || player.mediaStatus === MediaPlayer.BufferingMedia)
                                return i18n("Connecting…")
                            const t = player.metaData.stringValue(MediaMetaData.Title)
                            return t || (root.playing ? i18n("Playing") : i18n("Stopped"))
                        }
                    }
                }

                QQC2.ToolButton {
                    icon.name: root.playing ? "media-playback-stop" : "media-playback-start"
                    text: root.playing ? i18n("Stop") : i18n("Play")
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: root.togglePlayback()
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
                QQC2.ToolButton {
                    icon.name: audio.muted ? "audio-volume-muted" : "audio-volume-high"
                    text: i18n("Mute")
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: audio.muted
                    onToggled: audio.muted = checked
                }
                QQC2.Slider {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                    from: 0; to: 1
                    value: audio.volume
                    onMoved: { audio.volume = value; audio.muted = false }
                }
            }
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: root.togglePlayback()
    }
}
