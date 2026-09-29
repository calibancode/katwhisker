pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami
import org.kde.kradio

Kirigami.ApplicationWindow {
    id: root

    enum View { Discover, Favorites, Recent }

    title: current ? current.name : i18n("KRadio")
    width: Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 36
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: Kirigami.Units.gridUnit * 18

    // Stations are plain JS objects, see toStation() in radiobrowser.cpp.
    property var current: null
    property var favorites: []
    property var recent: []
    // The list the current station was started from; next/previous walk it.
    property var queue: []
    property int view: Main.View.Discover

    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property bool connecting: player.playbackState === MediaPlayer.PlayingState
        && (player.mediaStatus === MediaPlayer.LoadingMedia || player.mediaStatus === MediaPlayer.StalledMedia)
    readonly property string nowPlaying: player.metaData.stringValue(MediaMetaData.Title).trim()
    readonly property int queueIndex: current ? queue.findIndex(s => s.uuid === current.uuid) : -1

    function isFavorite(uuid) {
        return favorites.some(s => s.uuid === uuid)
    }
    function toggleFavorite(station) {
        favorites = isFavorite(station.uuid)
            ? favorites.filter(s => s.uuid !== station.uuid)
            : favorites.concat([station])
    }
    function play(station, fromList) {
        if (fromList)
            queue = fromList
        if (current && current.uuid === station.uuid && playing)
            return
        current = station
        recent = [station].concat(recent.filter(s => s.uuid !== station.uuid)).slice(0, 50)
        player.source = station.url
        player.play()
        RadioBrowser.countClick(station.uuid)
    }
    function resume() {
        if (!current || playing)
            return
        player.source = current.url // reconnect: a stopped live stream is stale
        player.play()
    }
    function togglePlayback() {
        // Live streams: stop instead of pausing into an ever-growing buffer.
        playing ? player.stop() : resume()
    }
    function step(delta) {
        if (queue.length === 0)
            return
        const i = queueIndex < 0 ? 0 : (queueIndex + delta + queue.length) % queue.length
        play(queue[i])
    }
    function browseTag(tag) {
        view = Main.View.Discover
        page.tag = tag
        page.query = ""
        page.reload()
    }
    function copyText(text, message) {
        clipboard.text = text
        clipboard.selectAll()
        clipboard.copy()
        showPassiveNotification(message)
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
        property alias view: root.view
        property alias query: page.query
        property alias tag: page.tag
        property alias order: page.order
        property string favoritesJson: "[]"
        property string recentJson: "[]"
        property string queueJson: "[]"
        property string currentJson: ""
        property bool wasPlaying: false
    }

    function parse(json, fallback) {
        try { return JSON.parse(json) ?? fallback } catch (e) { return fallback }
    }

    Component.onCompleted: {
        favorites = parse(settings.favoritesJson, [])
        recent = parse(settings.recentJson, [])
        queue = parse(settings.queueJson, [])
        current = parse(settings.currentJson, null)
        if (settings.wasPlaying)
            resume()
        page.reload()
    }
    onFavoritesChanged: settings.favoritesJson = JSON.stringify(favorites)
    onRecentChanged: settings.recentJson = JSON.stringify(recent)
    onQueueChanged: settings.queueJson = JSON.stringify(queue)
    onCurrentChanged: settings.currentJson = current ? JSON.stringify(current) : ""
    onClosing: settings.wasPlaying = playing

    MediaPlayer {
        id: player
        audioOutput: AudioOutput { id: audio; volume: 0.8 }
    }

    Mpris {
        playing: root.playing
        canPlay: root.current !== null
        canGoNext: root.queue.length > 1
        canGoPrevious: root.queue.length > 1
        volume: audio.muted ? 0 : audio.volume
        stationName: root.current?.name ?? ""
        title: root.nowPlaying
        artUrl: root.current?.favicon ?? ""
        trackId: root.current?.uuid ?? ""

        onRaiseRequested: { root.show(); root.raise(); root.requestActivate() }
        onQuitRequested: root.close()
        onPlayRequested: root.resume()
        onPauseRequested: player.stop()
        onStopRequested: player.stop()
        onPlayPauseRequested: root.togglePlayback()
        onNextRequested: root.step(1)
        onPreviousRequested: root.step(-1)
        onVolumeRequested: volume => { audio.volume = volume; audio.muted = false }
    }

    Connections {
        target: RadioBrowser
        function onVoted(uuid, ok, message) {
            root.showPassiveNotification(ok ? i18n("Thanks for voting!") : message)
        }
    }

    // Qt Quick has no clipboard API; a hidden TextEdit is the standard workaround.
    TextEdit { id: clipboard; visible: false }

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: page

        property string query: ""
        property string tag: ""
        property int order: RadioBrowser.Popular
        readonly property bool discover: root.view === Main.View.Discover
        readonly property var model: root.view === Main.View.Favorites ? root.favorites
                                   : root.view === Main.View.Recent ? root.recent
                                   : RadioBrowser.stations

        function reload() {
            RadioBrowser.search(query, tag, order)
            list.positionViewAtBeginning()
        }

        title: [i18n("Discover"), i18n("Favorites"), i18n("Recent")][root.view]

        titleDelegate: Kirigami.SearchField {
            id: searchField
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            text: page.query
            placeholderText: page.tag ? i18n("Search in “%1”…", page.tag) : i18n("Search stations…")
            delaySearch: true
            onAccepted: {
                if (text === page.query)
                    return
                page.query = text
                root.view = Main.View.Discover
                page.reload()
            }
            Shortcut {
                sequences: [StandardKey.Find]
                onActivated: searchField.forceActiveFocus()
            }
        }

        actions: [
            Kirigami.Action {
                icon.name: "view-sort"
                text: i18n("Sort")
                displayHint: Kirigami.DisplayHint.IconOnly
                visible: page.discover
                Kirigami.Action {
                    text: i18n("Most Popular"); checkable: true
                    checked: page.order === RadioBrowser.Popular
                    onTriggered: { page.order = RadioBrowser.Popular; page.reload() }
                }
                Kirigami.Action {
                    text: i18n("Trending"); checkable: true
                    checked: page.order === RadioBrowser.Trending
                    onTriggered: { page.order = RadioBrowser.Trending; page.reload() }
                }
                Kirigami.Action {
                    text: i18n("Top Voted"); checkable: true
                    checked: page.order === RadioBrowser.TopVoted
                    onTriggered: { page.order = RadioBrowser.TopVoted; page.reload() }
                }
            },
            Kirigami.Action {
                icon.name: "view-refresh"
                text: i18n("Refresh")
                displayHint: Kirigami.DisplayHint.AlwaysHide
                visible: page.discover
                shortcut: StandardKey.Refresh
                onTriggered: page.reload()
            },
            Kirigami.Action {
                icon.name: "clear-history"
                text: i18n("Clear History")
                displayHint: Kirigami.DisplayHint.AlwaysHide
                visible: root.view === Main.View.Recent && root.recent.length > 0
                onTriggered: root.recent = []
            }
        ]

        header: ColumnLayout {
            spacing: 0

            QQC2.TabBar {
                Layout.fillWidth: true
                currentIndex: root.view
                onCurrentIndexChanged: root.view = currentIndex
                QQC2.TabButton { text: i18n("Discover"); icon.name: "radio" }
                QQC2.TabButton { text: i18n("Favorites"); icon.name: "starred-symbolic" }
                QQC2.TabButton { text: i18n("Recent"); icon.name: "document-open-recent" }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                visible: page.discover && page.tag !== ""
                type: Kirigami.MessageType.Information
                icon.name: "tag"
                text: i18n("Showing stations tagged “%1”", page.tag)
                showCloseButton: true
                onVisibleChanged: if (!visible && page.tag !== "" && page.discover) { page.tag = ""; page.reload() }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                visible: page.discover && RadioBrowser.error !== "" && list.count > 0
                type: Kirigami.MessageType.Error
                text: RadioBrowser.error
                actions: Kirigami.Action {
                    text: i18n("Retry"); icon.name: "view-refresh"
                    onTriggered: page.reload()
                }
            }
        }

        ListView {
            id: list
            model: page.model
            reuseItems: true
            currentIndex: -1

            delegate: StationDelegate {
                id: stationDelegate
                width: ListView.view.width
                active: root.current !== null && root.current.uuid === stationDelegate.station.uuid
                playing: stationDelegate.active && root.playing
                favorite: root.isFavorite(stationDelegate.station.uuid)
                onPlayRequested: root.play(stationDelegate.station, page.model)
                onFavoriteToggled: root.toggleFavorite(stationDelegate.station)
                onTagClicked: tag => root.browseTag(tag)
                onVoteRequested: RadioBrowser.vote(stationDelegate.station.uuid)
                onCopyRequested: root.copyText(stationDelegate.station.url, i18n("Stream URL copied"))
            }

            onAtYEndChanged: if (atYEnd && page.discover && count > 0) RadioBrowser.fetchMore()

            footer: QQC2.BusyIndicator {
                width: ListView.view.width
                height: visible ? implicitHeight + Kirigami.Units.largeSpacing * 2 : 0
                visible: page.discover && RadioBrowser.busy && list.count > 0
                running: visible
            }

            Kirigami.LoadingPlaceholder {
                anchors.centerIn: parent
                visible: page.discover && RadioBrowser.busy && list.count === 0
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: list.count === 0 && !(page.discover && RadioBrowser.busy)
                readonly property bool failed: page.discover && RadioBrowser.error !== ""
                readonly property var content: {
                    if (failed)
                        return { icon: "network-disconnect", text: i18n("Couldn't reach radio-browser.info"), explanation: RadioBrowser.error }
                    switch (root.view) {
                    case Main.View.Favorites:
                        return { icon: "starred-symbolic", text: i18n("No favorites yet"), explanation: i18n("Star a station to keep it here.") }
                    case Main.View.Recent:
                        return { icon: "document-open-recent", text: i18n("Nothing played yet"), explanation: i18n("Stations you listen to show up here.") }
                    default:
                        return { icon: "radio", text: i18n("No stations found"), explanation: i18n("Try a different search.") }
                    }
                }
                icon.name: content.icon
                text: content.text
                explanation: content.explanation
                helpfulAction: failed ? retryAction : null

                Kirigami.Action {
                    id: retryAction
                    text: i18n("Retry"); icon.name: "view-refresh"
                    onTriggered: page.reload()
                }
            }
        }

        footer: PlayerBar {
            visible: root.current !== null
            station: root.current
            playing: root.playing
            connecting: root.connecting
            nowPlaying: root.nowPlaying
            errorString: player.error !== MediaPlayer.NoError ? player.errorString : ""
            favorite: root.current !== null && root.isFavorite(root.current.uuid)
            canStep: root.queue.length > 1
            audioOutput: audio

            onToggleRequested: root.togglePlayback()
            onNextRequested: root.step(1)
            onPreviousRequested: root.step(-1)
            onFavoriteToggled: root.toggleFavorite(root.current)
            onCopyTitleRequested: root.copyText(root.nowPlaying, i18n("Song title copied"))
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: !(root.activeFocusItem instanceof TextInput)
        onActivated: root.togglePlayback()
    }
    Shortcut { sequence: "Ctrl+Right"; onActivated: root.step(1) }
    Shortcut { sequence: "Ctrl+Left"; onActivated: root.step(-1) }
    Shortcut { sequence: "M"; enabled: !(root.activeFocusItem instanceof TextInput); onActivated: audio.muted = !audio.muted }
}
