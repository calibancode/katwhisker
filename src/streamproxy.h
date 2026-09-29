// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QTcpServer>
#include <QUrl>

#include "recordings.h"

#include <functional>
#include <optional>

class QNetworkReply;
class QTcpSocket;

// Rewrites Ogg granule positions so they start near zero, and reports the
// artist/title from Vorbis comment headers.
//
// Icecast relays Ogg streams (FLAC, Vorbis, Opus) with the granule positions
// of a stream that may have been running for days. FFmpeg then stamps the
// first packets at 0 and the rest at e.g. 70 hours, and Qt Multimedia waits
// for that timestamp: you hear a split second, then silence. Non-Ogg data is
// passed through untouched.
class OggRebaser
{
public:
    std::function<void(const QString &artist, const QString &title)> onTags;
    // A new logical stream (in Icecast, usually a new song) starts at this
    // offset of the output being produced.
    std::function<void(qsizetype offset)> onStreamStart;

    QByteArray process(const QByteArray &data);
    // File extension for the codec, once the first stream header was seen.
    QString extension() const { return m_extension; }

    bool isOgg() const { return m_mode == Ogg; }
    bool isDecided() const { return m_mode != Unknown; }
    // Seconds of audio emitted so far, from the rewritten granule positions;
    // nullopt until the sample rate is known.
    std::optional<double> emittedSeconds() const;

private:
    struct Pending {
        QByteArray page;
        qint64 granule;
        QByteArray following; // pages that arrived while holding, in order
    };
    void emitPage(QByteArray page, QByteArray &out);
    void parseTags(QByteArrayView page);

    QByteArray m_in;
    enum { Unknown, Ogg, Passthrough } m_mode = Unknown;
    // Per logical stream: the first audio page is held back until the next
    // one tells us roughly how many samples a page carries.
    std::optional<Pending> m_held;
    std::optional<qint64> m_base;
    qint64 m_lastOut = 0;
    std::optional<qint64> m_firstOut;
    QString m_extension;
    int m_sampleRate = 0;
    int m_fixedBlockSize = 0; // FLAC only
};

// Removes SHOUTcast/Icecast in-band metadata ("icy-metaint") from a stream
// and reports each StreamTitle.
class IcyStripper
{
public:
    explicit IcyStripper(int metaInt)
        : m_metaInt(metaInt)
        , m_untilMeta(metaInt)
    {
    }

    // Called with each new StreamTitle and where the next song's audio
    // starts in the output being produced.
    std::function<void(const QString &streamTitle, qsizetype offset)> onTitle;

    QByteArray process(const QByteArray &data);

private:
    int m_metaInt;
    int m_untilMeta;
    int m_metaLen = -1; // -1: expecting the length byte
    QByteArray m_meta;
    QString m_lastTitle;
};

// Serves radio streams through 127.0.0.1 so they can be fixed up on the fly
// and so we can read the song metadata that Qt Multimedia doesn't expose.
class StreamProxy : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(RecordingsModel *recordings READ recordings CONSTANT)

public:
    explicit StreamProxy(QObject *parent = nullptr);

    QString artist() const { return m_artist; }
    QString title() const { return m_title; }
    RecordingsModel *recordings() { return &m_recordings; }

    // Returns a URL the media player should open instead of `url`. HLS
    // playlists are returned unchanged, since their segment URLs are relative.
    Q_INVOKABLE QUrl wrap(const QUrl &url, bool hls, const QString &stationName, const QString &favicon);
    // Ends all player connections. The player's reader thread blocks on our
    // socket, and its teardown blocks the UI thread we serve from, so
    // connections must end before the player lets go of a stream.
    Q_INVOKABLE void closeConnections();

Q_SIGNALS:
    void metadataChanged();

private:
    void handleConnection(QTcpSocket *socket);
    struct Target {
        QUrl url;
        int generation = 0;
        QString station;
        QString favicon;
    };
    void startSession(QTcpSocket *socket, const Target &target);
    void setMetadata(int generation, const QString &artist, const QString &title);
    void pollIcecastStatus(const QUrl &streamUrl, int generation);

    QTcpServer m_server;
    QNetworkAccessManager m_nam;
    QString m_targetId;
    Target m_target;
    RecordingsModel m_recordings;
    int m_generation = 0;
    QString m_artist;
    QString m_title;
};
