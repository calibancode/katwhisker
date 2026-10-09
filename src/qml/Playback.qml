// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

import QtMultimedia

MediaPlayer {
    id: player
    required property var controls
    readonly property bool playing: playbackState === MediaPlayer.PlayingState
    readonly property bool connecting: playing && (mediaStatus === MediaPlayer.LoadingMedia || mediaStatus === MediaPlayer.StalledMedia)
    readonly property string title: metaData.stringValue(MediaMetaData.Title).trim()
    // Live streams never end on their own: reaching the end means the
    // connection was cut (e.g. by the proxy dropping a stalled stream).
    readonly property string problem: error !== MediaPlayer.NoError ? errorString
        : mediaStatus === MediaPlayer.EndOfMedia ? i18n("Connection lost") : ""

    audioOutput: AudioOutput {
        volume: player.controls.volume
        muted: player.controls.muted
    }
}
