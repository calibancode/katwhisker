pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kradio

Kirigami.ApplicationWindow {
    id: root

    enum View {
        Discover,
        Favorites,
        Recent,
        Recordings
    }

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
    readonly property bool connecting: player.playbackState === MediaPlayer.PlayingState && (player.mediaStatus === MediaPlayer.LoadingMedia || player.mediaStatus === MediaPlayer.StalledMedia)
    readonly property string nowPlaying: StreamProxy.title ? (StreamProxy.artist ? i18nc("artist – song title", "%1 – %2", StreamProxy.artist, StreamProxy.title) : StreamProxy.title) : player.metaData.stringValue(MediaMetaData.Title).trim()
    readonly property int queueIndex: current ? queue.findIndex(s => s.uuid === current.uuid) : -1

    function isFavorite(uuid) {
        return favorites.some(s => s.uuid === uuid);
    }
    function toggleFavorite(station) {
        favorites = isFavorite(station.uuid) ? favorites.filter(s => s.uuid !== station.uuid) : favorites.concat([station]);
    }
    function play(station, fromList) {
        if (fromList) {
            // Discover lists grow as you scroll; keep a window around the
            // station so the persisted queue stays small.
            const i = Math.max(0, fromList.findIndex(s => s.uuid === station.uuid));
            queue = fromList.slice(Math.max(0, i - 100), i + 100);
        }
        if (current && current.uuid === station.uuid && playing)
            return;
        current = station;
        recent = [station].concat(recent.filter(s => s.uuid !== station.uuid)).slice(0, 50);
        player.source = streamSource(station);
        player.play();
        RadioBrowser.countClick(station.uuid);
    }
    function resume() {
        if (!current || playing)
            return;
        player.source = streamSource(current); // reconnect: a stopped live stream is stale
        player.play();
    }
    function streamSource(station) {
        return StreamProxy.wrap(station.url, station.hls ?? false, station.name, station.favicon ?? "");
    }
    function togglePlayback() {
        // Live streams: stop instead of pausing into an ever-growing buffer.
        playing ? player.stop() : resume();
    }
    function step(delta) {
        if (queue.length === 0)
            return;
        const i = queueIndex < 0 ? 0 : (queueIndex + delta + queue.length) % queue.length;
        play(queue[i]);
    }
    function browseTag(tag) {
        view = Main.View.Discover;
        page.tag = tag;
        page.query = "";
        page.reload();
    }
    function copyText(text, message) {
        clipboard.text = text;
        clipboard.selectAll();
        clipboard.copy();
        showPassiveNotification(message);
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
        property alias favoritesByName: page.favoritesByName
        property string favoritesJson: "[]"
        property string recentJson: "[]"
        property string queueJson: "[]"
        property string currentJson: ""
        property bool wasPlaying: false
    }

    function parse(json, fallback) {
        try {
            return JSON.parse(json) ?? fallback;
        } catch (e) {
            return fallback;
        }
    }

    Component.onCompleted: {
        favorites = parse(settings.favoritesJson, []);
        recent = parse(settings.recentJson, []);
        queue = parse(settings.queueJson, []);
        current = parse(settings.currentJson, null);
        if (settings.wasPlaying)
            resume();
        page.reload();
    }
    onFavoritesChanged: settings.favoritesJson = JSON.stringify(favorites)
    onRecentChanged: settings.recentJson = JSON.stringify(recent)
    onQueueChanged: settings.queueJson = JSON.stringify(queue)
    onCurrentChanged: settings.currentJson = current ? JSON.stringify(current) : ""
    onClosing: settings.wasPlaying = playing

    MediaPlayer {
        id: player
        audioOutput: AudioOutput {
            id: audio
            volume: 0.8
        }
    }

    Mpris {
        playing: root.playing
        canPlay: root.current !== null
        canGoNext: root.queue.length > 1
        canGoPrevious: root.queue.length > 1
        volume: audio.muted ? 0 : audio.volume
        stationName: root.current?.name ?? ""
        title: StreamProxy.title || root.nowPlaying
        artist: StreamProxy.artist
        artUrl: root.current?.favicon ?? ""
        trackId: root.current?.uuid ?? ""

        onRaiseRequested: {
            root.show();
            root.raise();
            root.requestActivate();
        }
        onQuitRequested: root.close()
        onPlayRequested: root.resume()
        onPauseRequested: player.stop()
        onStopRequested: player.stop()
        onPlayPauseRequested: root.togglePlayback()
        onNextRequested: root.step(1)
        onPreviousRequested: root.step(-1)
        onVolumeRequested: volume => {
            audio.volume = volume;
            audio.muted = false;
        }
    }

    Connections {
        target: RadioBrowser
        function onVoted(uuid, ok, message) {
            root.showPassiveNotification(ok ? i18n("Thanks for voting!") : message);
        }
    }

    FileDialog {
        id: exportDialog
        property int row: -1
        fileMode: FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.MusicLocation)
        onAccepted: {
            const error = StreamProxy.recordings.exportTo(row, selectedFile);
            root.showPassiveNotification(error ? i18n("Couldn't export: %1", error) : i18n("Recording exported"));
        }
    }

    // Qt Quick has no clipboard API; a hidden TextEdit is the standard workaround.
    TextEdit {
        id: clipboard
        visible: false
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: page

        // Kirigami forwards keys to the list's current item through plain
        // pointers; when rows are rebuilt (new search, tab switch) that item
        // can be deleted first and the next key press crashes. The list still
        // handles arrow keys itself once focused.
        keyboardNavigationEnabled: false

        property string query: ""
        property string tag: ""
        property int order: RadioBrowser.Popular
        readonly property bool discover: root.view === Main.View.Discover
        property bool favoritesByName: false
        // Local filter for Favorites and Recent; Discover searches the server.
        property string filter: ""
        readonly property bool recordingsView: root.view === Main.View.Recordings
        // Everything that differs per tab in the toolbar, in one place.
        readonly property var viewConfig: {
            const popupSort = menu => () => menu.popup(viewAction, 0, viewAction.height);
            switch (root.view) {
            case Main.View.Favorites:
                return {
                    placeholder: i18n("Search favorites…"),
                    actionIcon: "view-sort",
                    actionText: i18n("Sort"),
                    actionEnabled: true,
                    action: popupSort(favoritesSortMenu)
                };
            case Main.View.Recent:
                return {
                    placeholder: i18n("Search recent…"),
                    actionIcon: "edit-clear-history",
                    actionText: i18n("Clear History"),
                    actionEnabled: root.recent.length > 0,
                    action: () => root.recent = []
                };
            case Main.View.Recordings:
                return {
                    placeholder: i18n("Search recordings…"),
                    actionIcon: "edit-clear-all",
                    actionText: i18n("Discard All"),
                    actionEnabled: StreamProxy.recordings.count > 0,
                    action: () => StreamProxy.recordings.clear()
                };
            default:
                return {
                    placeholder: tag ? i18n("Search in “%1”…", tag) : i18n("Search stations…"),
                    actionIcon: "view-sort",
                    actionText: i18n("Sort"),
                    actionEnabled: true,
                    action: popupSort(discoverSortMenu)
                };
            }
        }
        readonly property var model: {
            if (discover)
                return RadioBrowser.stations;
            if (recordingsView)
                return StreamProxy.recordings; // filtered in its delegate
            let list = root.view === Main.View.Favorites ? root.favorites : root.recent;
            if (root.view === Main.View.Favorites && favoritesByName)
                list = list.slice().sort((a, b) => a.name.localeCompare(b.name));
            const needle = filter.trim().toLowerCase();
            if (needle)
                list = list.filter(s => s.name.toLowerCase().includes(needle) || (Array.isArray(s.tags) && s.tags.some(t => t.toLowerCase().includes(needle))));
            return list;
        }

        function reload() {
            RadioBrowser.search(query, tag, order);
            list.positionViewAtBeginning();
        }

        title: [i18n("Discover"), i18n("Favorites"), i18n("Recent")][root.view]

        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        header: ColumnLayout {
            spacing: 0

            // Our own toolbar instead of Kirigami's page header: its actions area
            // always fills width, which leaves uneven space around our button.
            QQC2.ToolBar {
                Layout.fillWidth: true
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        text: page.discover ? page.query : page.filter
                        placeholderText: page.viewConfig.placeholder
                        delaySearch: true
                        onAccepted: {
                            if (!page.discover) {
                                page.filter = text;
                            } else if (text !== page.query) {
                                page.query = text;
                                page.reload();
                            }
                        }
                        // Typing breaks the text binding; restore the tab's own query on switch.
                        Connections {
                            target: root
                            function onViewChanged() {
                                page.filter = "";
                                searchField.text = page.discover ? page.query : "";
                            }
                        }
                        // browseTag() and friends change the query behind our back.
                        Connections {
                            target: page
                            function onQueryChanged() {
                                if (page.discover)
                                    searchField.text = page.query;
                            }
                        }
                        Shortcut {
                            sequences: [StandardKey.Find]
                            onActivated: searchField.forceActiveFocus()
                        }
                    }

                    IconToolButton {
                        id: viewAction
                        icon.name: page.viewConfig.actionIcon
                        text: page.viewConfig.actionText
                        enabled: page.viewConfig.actionEnabled
                        onClicked: page.viewConfig.action()

                        QQC2.Menu {
                            id: discoverSortMenu
                            QQC2.ActionGroup {
                                id: discoverSortGroup
                            }
                            QQC2.MenuItem {
                                action: QQC2.Action {
                                    QQC2.ActionGroup.group: discoverSortGroup
                                    text: i18n("Most Popular")
                                    checkable: true
                                    checked: page.order === RadioBrowser.Popular
                                    onTriggered: {
                                        page.order = RadioBrowser.Popular;
                                        page.reload();
                                    }
                                }
                            }
                            QQC2.MenuItem {
                                action: QQC2.Action {
                                    QQC2.ActionGroup.group: discoverSortGroup
                                    text: i18n("Trending")
                                    checkable: true
                                    checked: page.order === RadioBrowser.Trending
                                    onTriggered: {
                                        page.order = RadioBrowser.Trending;
                                        page.reload();
                                    }
                                }
                            }
                            QQC2.MenuItem {
                                action: QQC2.Action {
                                    QQC2.ActionGroup.group: discoverSortGroup
                                    text: i18n("Top Voted")
                                    checkable: true
                                    checked: page.order === RadioBrowser.TopVoted
                                    onTriggered: {
                                        page.order = RadioBrowser.TopVoted;
                                        page.reload();
                                    }
                                }
                            }
                        }
                        QQC2.Menu {
                            id: favoritesSortMenu
                            QQC2.ActionGroup {
                                id: favoritesSortGroup
                            }
                            QQC2.MenuItem {
                                action: QQC2.Action {
                                    QQC2.ActionGroup.group: favoritesSortGroup
                                    text: i18n("Order Added")
                                    checkable: true
                                    checked: !page.favoritesByName
                                    onTriggered: page.favoritesByName = false
                                }
                            }
                            QQC2.MenuItem {
                                action: QQC2.Action {
                                    QQC2.ActionGroup.group: favoritesSortGroup
                                    text: i18n("Name")
                                    checkable: true
                                    checked: page.favoritesByName
                                    onTriggered: page.favoritesByName = true
                                }
                            }
                        }
                    }
                }
            }

            QQC2.TabBar {
                Layout.fillWidth: true
                currentIndex: root.view
                onCurrentIndexChanged: root.view = currentIndex
                QQC2.TabButton {
                    text: i18n("Discover")
                }
                QQC2.TabButton {
                    text: i18n("Favorites")
                }
                QQC2.TabButton {
                    text: i18n("Recent")
                }
                QQC2.TabButton {
                    text: i18n("Recordings")
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                visible: page.discover && page.tag !== ""
                type: Kirigami.MessageType.Information
                icon.name: "tag"
                text: i18n("Showing stations tagged “%1”", page.tag)
                // An action rather than the close button: closing assigns
                // visible = false, which would break the binding above for good.
                actions: Kirigami.Action {
                    text: i18n("Show All")
                    icon.name: "edit-clear"
                    onTriggered: {
                        page.tag = "";
                        page.reload();
                    }
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                visible: page.discover && RadioBrowser.error !== "" && list.count > 0
                type: Kirigami.MessageType.Error
                text: RadioBrowser.error
                actions: Kirigami.Action {
                    text: i18n("Retry")
                    icon.name: "view-refresh"
                    onTriggered: page.reload()
                }
            }
        }

        ListView {
            id: list
            model: page.model
            reuseItems: true
            currentIndex: -1

            Component {
                id: stationComponent
            StationDelegate {
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
            }

            Component {
                id: recordingComponent
                RecordingDelegate {
                    id: recordingDelegate
                    readonly property bool matches: !page.filter || (recordingDelegate.title + " " + recordingDelegate.artist).toLowerCase().includes(page.filter.trim().toLowerCase())
                    width: ListView.view.width
                    visible: matches
                    height: matches ? implicitHeight : 0
                    onExportRequested: {
                        exportDialog.row = recordingDelegate.index;
                        exportDialog.selectedFile = exportDialog.currentFolder + "/" + encodeURIComponent(recordingDelegate.fileName);
                        exportDialog.open();
                    }
                    onDiscardRequested: StreamProxy.recordings.remove(recordingDelegate.index)
                }
            }

            delegate: page.recordingsView ? recordingComponent : stationComponent

            onAtYEndChanged: if (atYEnd && page.discover && count > 0)
                RadioBrowser.fetchMore()

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
                        return {
                            icon: "network-disconnect",
                            text: i18n("Couldn't reach radio-browser.info"),
                            explanation: RadioBrowser.error
                        };
                    switch (root.view) {
                    case Main.View.Favorites:
                        if (page.filter)
                            return {
                                icon: "search",
                                text: i18n("No matching favorites"),
                                explanation: ""
                            };
                        return {
                            icon: "starred-symbolic",
                            text: i18n("No favorites yet"),
                            explanation: ""
                        };
                    case Main.View.Recent:
                        if (page.filter)
                            return {
                                icon: "search",
                                text: i18n("No matching stations"),
                                explanation: ""
                            };
                        return {
                            icon: "document-open-recent",
                            text: i18n("Nothing played yet"),
                            explanation: ""
                        };
                    case Main.View.Recordings:
                        return {
                            icon: "media-record",
                            text: i18n("No recordings yet"),
                            explanation: ""
                        };
                    default:
                        return {
                            icon: "radio",
                            text: i18n("No stations found"),
                            explanation: ""
                        };
                    }
                }
                icon.name: content.icon
                text: content.text
                explanation: content.explanation
                helpfulAction: failed ? retryAction : null

                Kirigami.Action {
                    id: retryAction
                    text: i18n("Retry")
                    icon.name: "view-refresh"
                    onTriggered: page.reload()
                }
            }
        }

        footer: PlayerBar {
            visible: root.current !== null
            recordingTitle: StreamProxy.recordings.recordingTitle
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
    Shortcut {
        sequences: [StandardKey.Refresh]
        enabled: page.discover
        onActivated: page.reload()
    }
    Shortcut {
        sequence: "Ctrl+Right"
        onActivated: root.step(1)
    }
    Shortcut {
        sequence: "Ctrl+Left"
        onActivated: root.step(-1)
    }
    Shortcut {
        sequence: "M"
        enabled: !(root.activeFocusItem instanceof TextInput)
        onActivated: audio.muted = !audio.muted
    }
}
