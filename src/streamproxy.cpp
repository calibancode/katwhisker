#include "streamproxy.h"

#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <memory>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr int HeaderSize = 27;

constexpr auto crcTable = [] {
    std::array<quint32, 256> table{};
    for (quint32 i = 0; i < 256; ++i) {
        quint32 r = i << 24;
        for (int j = 0; j < 8; ++j)
            r = (r & 0x80000000u) ? (r << 1) ^ 0x04c11db7u : r << 1;
        table[i] = r;
    }
    return table;
}();

quint32 oggCrc(const QByteArray &page)
{
    quint32 crc = 0;
    for (const auto c : page)
        crc = (crc << 8) ^ crcTable[((crc >> 24) ^ quint8(c)) & 0xff];
    return crc;
}

qint64 granuleOf(const QByteArray &page)
{
    return qFromLittleEndian<qint64>(page.constData() + 6);
}

void setGranule(QByteArray &page, qint64 granule)
{
    qToLittleEndian<qint64>(granule, page.data() + 6);
    qToLittleEndian<quint32>(0, page.data() + 22);
    qToLittleEndian<quint32>(oggCrc(page), page.data() + 22);
}
}

QByteArray OggRebaser::process(const QByteArray &data)
{
    if (m_mode == Passthrough)
        return data;

    m_in += data;
    if (m_mode == Unknown) {
        if (m_in.size() < 4)
            return {};
        m_mode = m_in.startsWith("OggS") ? Ogg : Passthrough;
        if (m_mode == Passthrough)
            return std::exchange(m_in, {});
    }

    QByteArray out;
    qsizetype pos = 0;
    while (m_in.size() - pos >= HeaderSize) {
        if (!QByteArrayView(m_in).sliced(pos).startsWith("OggS")) {
            // Lost sync: skip ahead to the next capture pattern.
            const auto next = m_in.indexOf("OggS", pos + 1);
            pos = next < 0 ? m_in.size() - 3 : next;
            continue;
        }
        const int segments = quint8(m_in[pos + 26]);
        if (m_in.size() - pos < HeaderSize + segments)
            break;
        qsizetype size = HeaderSize + segments;
        for (int i = 0; i < segments; ++i)
            size += quint8(m_in[pos + HeaderSize + i]);
        if (m_in.size() - pos < size)
            break;
        emitPage(m_in.sliced(pos, size), out);
        pos += size;
    }
    m_in.remove(0, pos);
    return out;
}

void OggRebaser::emitPage(QByteArray page, QByteArray &out)
{
    const bool bos = quint8(page[5]) & 0x02;
    const qint64 granule = granuleOf(page);

    if (bos) {
        // A new logical stream (Icecast chains one per track): rebase it to
        // continue where the previous one left off.
        if (m_held) {
            setGranule(m_held->page, m_lastOut);
            out += m_held->page + m_held->following;
            m_held.reset();
        }
        m_base.reset();
    }

    // Header pages carry granule 0 and pages without a finished packet -1;
    // neither needs rewriting.
    if (granule <= 0) {
        if (m_held)
            m_held->following += page;
        else
            out += page;
        return;
    }

    if (!m_base) {
        if (!m_held) {
            m_held = Pending{page, granule, {}};
            return;
        }
        const qint64 perPage = qMax<qint64>(0, granule - m_held->granule);
        m_base = m_held->granule - m_lastOut - perPage;
        setGranule(m_held->page, m_held->granule - *m_base);
        out += m_held->page + m_held->following;
        m_held.reset();
    }

    m_lastOut = granule - *m_base;
    setGranule(page, m_lastOut);
    out += page;
}

StreamProxy::StreamProxy(QObject *parent)
    : QObject(parent)
{
    m_nam.setTransferTimeout(15000);
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (auto *socket = m_server.nextPendingConnection())
            handleConnection(socket);
    });
}

QUrl StreamProxy::wrap(const QUrl &url, const QString &codec)
{
    static const QStringList oggCodecs{u"FLAC"_s, u"OGG"_s, u"OPUS"_s, u"VORBIS"_s};
    const bool ogg = std::ranges::any_of(oggCodecs, [&](const auto &c) {
        return codec.contains(c, Qt::CaseInsensitive);
    });
    if (!ogg || !url.isValid())
        return url;
    if (!m_server.isListening() && !m_server.listen(QHostAddress::LocalHost))
        return url;

    const auto id = QString::number(QRandomGenerator::global()->generate64(), 36);
    m_urls.insert(id, url);
    return QUrl(u"http://127.0.0.1:%1/%2"_s.arg(m_server.serverPort()).arg(id));
}

void StreamProxy::handleConnection(QTcpSocket *socket)
{
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        if (!socket->canReadLine() || socket->property("started").toBool())
            return;
        socket->setProperty("started", true);

        // "GET /<id> HTTP/1.1"
        const auto parts = socket->readLine().split(' ');
        const auto id = parts.size() >= 2 ? QString::fromLatin1(parts[1].mid(1)) : QString();
        const auto upstream = m_urls.value(id);
        if (upstream.isEmpty()) {
            socket->write("HTTP/1.0 404 Not Found\r\n\r\n");
            socket->disconnectFromHost();
            return;
        }

        QNetworkRequest req(upstream);
        req.setHeader(QNetworkRequest::UserAgentHeader, u"KRadio/0.1"_s);
        req.setTransferTimeout(0); // live streams never finish
        auto *reply = m_nam.get(req);
        auto rebaser = std::make_shared<OggRebaser>();

        connect(socket, &QTcpSocket::disconnected, reply, [reply] {
            reply->abort();
            reply->deleteLater();
        });
        connect(reply, &QNetworkReply::metaDataChanged, socket, [socket, reply, headerSent = false]() mutable {
            // Skip redirect hops; answer once for the final response.
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() / 100 == 3 || headerSent)
                return;
            headerSent = true;
            const auto type = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
            socket->write("HTTP/1.0 200 OK\r\nContent-Type: " + (type.isEmpty() ? "application/ogg" : type)
                          + "\r\nCache-Control: no-cache\r\n\r\n");
        });
        connect(reply, &QNetworkReply::readyRead, socket, [socket, reply, rebaser] {
            socket->write(rebaser->process(reply->readAll()));
        });
        connect(reply, &QNetworkReply::finished, socket, [socket] {
            socket->disconnectFromHost();
        });
    });
}
