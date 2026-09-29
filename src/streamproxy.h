#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QTcpServer>
#include <QUrl>

#include <optional>

// Rewrites Ogg granule positions so they start near zero.
//
// Icecast relays Ogg streams (FLAC, Vorbis, Opus) with the granule positions
// of a stream that may have been running for days. FFmpeg then stamps the
// first packets at 0 and the rest at e.g. 70 hours, and Qt Multimedia waits
// for that timestamp: you hear a split second, then silence. Non-Ogg data is
// passed through untouched.
class OggRebaser
{
public:
    QByteArray process(const QByteArray &data);

private:
    struct Pending {
        QByteArray page;
        qint64 granule;
        QByteArray following; // pages that arrived while holding, in order
    };
    void emitPage(QByteArray page, QByteArray &out);

    QByteArray m_in;
    enum { Unknown, Ogg, Passthrough } m_mode = Unknown;
    // Per logical stream: the first audio page is held back until the next
    // one tells us roughly how many samples a page carries.
    std::optional<Pending> m_held;
    std::optional<qint64> m_base;
    qint64 m_lastOut = 0;
};

// Serves radio streams through 127.0.0.1 so they can be fixed up on the fly.
class StreamProxy : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit StreamProxy(QObject *parent = nullptr);

    // Returns a URL the media player should open instead of `url`. Only Ogg
    // based codecs are proxied; everything else is returned unchanged.
    Q_INVOKABLE QUrl wrap(const QUrl &url, const QString &codec);

private:
    void handleConnection(QTcpSocket *socket);

    QTcpServer m_server;
    QNetworkAccessManager m_nam;
    QHash<QString, QUrl> m_urls;
};
