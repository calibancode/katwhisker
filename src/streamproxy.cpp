#include "streamproxy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
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
constexpr double CushionSeconds = 2.0;

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

// "Artist - Title" is the de facto StreamTitle convention. Some stations
// use "Title by Artist - Station", so check for that first.
std::pair<QString, QString> splitStreamTitle(const QString &streamTitle)
{
    const auto dash = streamTitle.indexOf(u" - "_s);
    const auto by = streamTitle.indexOf(u" by "_s);
    if (by > 0 && (dash < 0 || by < dash)) {
        const auto artistEnd = dash < 0 ? streamTitle.size() : dash;
        return {streamTitle.sliced(by + 4, artistEnd - by - 4).trimmed(), streamTitle.first(by).trimmed()};
    }
    if (dash < 0)
        return {{}, streamTitle.trimmed()};
    return {streamTitle.first(dash).trimmed(), streamTitle.sliced(dash + 3).trimmed()};
}

QString decodeText(const QByteArray &bytes)
{
    // Mostly UTF-8, but older SHOUTcast servers send Latin-1.
    auto decoder = QStringDecoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = decoder(bytes);
    return decoder.hasError() ? QString::fromLatin1(bytes) : text;
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

std::optional<double> OggRebaser::emittedSeconds() const
{
    if (m_sampleRate <= 0 || !m_firstOut)
        return {};
    return double(m_lastOut - *m_firstOut) / m_sampleRate;
}

void OggRebaser::emitPage(QByteArray page, QByteArray &out)
{
    const bool bos = quint8(page[5]) & 0x02;
    const qint64 granule = granuleOf(page);

    if (bos) {
        // Identification header: learn the sample rate granules count in.
        const auto body = QByteArrayView(page).sliced(HeaderSize + quint8(page[26]));
        if (body.startsWith("\x7f" "FLAC") && body.size() >= 30) {
            // "\x7fFLAC", version, header count, "fLaC", block header, STREAMINFO
            const auto *si = reinterpret_cast<const quint8 *>(body.data()) + 17;
            m_sampleRate = (si[10] << 12) | (si[11] << 4) | (si[12] >> 4);
            const int minBlock = (si[0] << 8) | si[1];
            const int maxBlock = (si[2] << 8) | si[3];
            m_fixedBlockSize = minBlock == maxBlock ? minBlock : 0;
            m_extension = u"oga"_s;
        } else if (body.startsWith("\x01vorbis") && body.size() >= 16) {
            m_sampleRate = qFromLittleEndian<quint32>(body.data() + 12);
            m_extension = u"ogg"_s;
        } else if (body.startsWith("OpusHead")) {
            m_sampleRate = 48000; // Opus granules are always 48 kHz
            m_extension = u"opus"_s;
        }
    }

    if (bos) {
        // A new logical stream (Icecast chains one per track): rebase it to
        // continue where the previous one left off.
        if (m_held) {
            setGranule(m_held->page, m_lastOut);
            out += m_held->page + m_held->following;
            m_held.reset();
        }
        m_base.reset();
        if (onStreamStart)
            onStreamStart(out.size());
    }

    // Header pages carry granule 0 and pages without a finished packet -1;
    // neither needs rewriting.
    if (granule <= 0) {
        if (granule == 0 && !bos)
            parseTags(page);
        if (m_held)
            m_held->following += page;
        else
            out += page;
        return;
    }

    // Icecast joins mid-stream, so the first audio page usually starts with
    // the tail of a packet we never got. Skip pages until one starts cleanly;
    // otherwise the fragment throws off FFmpeg's first timestamps.
    if (!m_base && !m_held && (quint8(page[5]) & 0x01))
        return;

    if (!m_base && !m_held && m_fixedBlockSize > 0) {
        // Fixed-blocksize FLAC: one packet per frame, so the page's exact
        // sample count is its packet count (lacing values < 255) × blocksize.
        const int segments = quint8(page[26]);
        qint64 packets = 0;
        for (int i = 0; i < segments; ++i)
            packets += quint8(page[HeaderSize + i]) < 255;
        m_base = granule - m_lastOut - packets * m_fixedBlockSize;
    }

    if (!m_base) {
        if (!m_held) {
            m_held = Pending{page, granule, {}};
            return;
        }
        // Estimate the held page's samples from the next page's; only exact
        // when pages carry equal sample counts.
        const qint64 perPage = qMax<qint64>(0, granule - m_held->granule);
        m_base = m_held->granule - m_lastOut - perPage;
        setGranule(m_held->page, m_held->granule - *m_base);
        out += m_held->page + m_held->following;
        m_held.reset();
    }

    m_lastOut = granule - *m_base;
    if (!m_firstOut)
        m_firstOut = m_lastOut;
    setGranule(page, m_lastOut);
    out += page;
}

void OggRebaser::parseTags(QByteArrayView page)
{
    if (!onTags)
        return;
    const int segments = quint8(page[26]);
    auto body = page.sliced(HeaderSize + segments);

    // The comment header of each Ogg mapping, then a Vorbis comment block.
    if (body.startsWith("\x03vorbis"))
        body = body.sliced(7);
    else if (body.startsWith("OpusTags"))
        body = body.sliced(8);
    else if (!body.isEmpty() && (quint8(body[0]) & 0x7f) == 4) // FLAC VORBIS_COMMENT block
        body = body.sliced(4);
    else
        return;

    qsizetype pos = 0;
    auto readU32 = [&]() -> std::optional<quint32> {
        if (body.size() - pos < 4)
            return {};
        const auto v = qFromLittleEndian<quint32>(body.data() + pos);
        pos += 4;
        return v;
    };
    const auto vendorLen = readU32();
    if (!vendorLen || body.size() - pos < *vendorLen)
        return;
    pos += *vendorLen;
    const auto count = readU32();
    if (!count)
        return;

    QString artist, title;
    for (quint32 i = 0; i < *count; ++i) {
        const auto len = readU32();
        if (!len || body.size() - pos < *len)
            break;
        const auto comment = QString::fromUtf8(body.sliced(pos, *len));
        pos += *len;
        const auto eq = comment.indexOf(u'=');
        const auto key = comment.first(qMax(0, eq)).toUpper();
        if (key == "ARTIST"_L1)
            artist = comment.sliced(eq + 1);
        else if (key == "TITLE"_L1)
            title = comment.sliced(eq + 1);
    }
    if (!title.isEmpty() || !artist.isEmpty())
        onTags(artist, title);
}

QByteArray IcyStripper::process(const QByteArray &data)
{
    QByteArray out;
    out.reserve(data.size());
    qsizetype pos = 0;
    while (pos < data.size()) {
        if (m_untilMeta > 0) {
            const auto n = qMin<qsizetype>(m_untilMeta, data.size() - pos);
            out += data.sliced(pos, n);
            pos += n;
            m_untilMeta -= n;
        } else if (m_metaLen < 0) {
            m_metaLen = quint8(data[pos++]) * 16;
            m_meta.clear();
        } else {
            const auto n = qMin<qsizetype>(m_metaLen - m_meta.size(), data.size() - pos);
            m_meta += data.sliced(pos, n);
            pos += n;
        }

        if (m_untilMeta == 0 && m_metaLen >= 0 && m_meta.size() == m_metaLen) {
            // e.g. "StreamTitle='Artist - Title';StreamUrl='';" padded with NULs
            static constexpr QByteArrayView key = "StreamTitle='";
            if (const auto start = m_meta.indexOf(key); start >= 0 && onTitle) {
                const auto from = start + key.size();
                auto end = m_meta.indexOf("';", from);
                if (end < 0)
                    end = m_meta.lastIndexOf('\'');
                // Servers repeat the title in every block; only changes matter.
                if (end >= from) {
                    const auto title = decodeText(m_meta.sliced(from, end - from));
                    if (title != std::exchange(m_lastTitle, title))
                        onTitle(title, out.size());
                }
            }
            m_metaLen = -1;
            m_untilMeta = m_metaInt;
        }
    }
    return out;
}

StreamProxy::StreamProxy(QObject *parent)
    : QObject(parent)
{
    m_nam.setRedirectPolicy(QNetworkRequest::UserVerifiedRedirectPolicy);
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (auto *socket = m_server.nextPendingConnection())
            handleConnection(socket);
    });
}

QUrl StreamProxy::wrap(const QUrl &url, bool hls, const QString &stationName)
{
    // A new station: forget the previous song, and let old sessions go stale.
    ++m_generation;
    setMetadata(m_generation, {}, {});
    m_urls.clear(); // players reconnect through the latest URL only

    if (hls || !url.isValid() || !url.scheme().startsWith(u"http"_s))
        return url;
    if (!m_server.isListening() && !m_server.listen(QHostAddress::LocalHost))
        return url;

    const auto id = QString::number(QRandomGenerator::global()->generate64(), 36);
    m_urls.insert(id, {url, m_generation, stationName});
    return QUrl(u"http://127.0.0.1:%1/%2"_s.arg(m_server.serverPort()).arg(id));
}

void StreamProxy::setMetadata(int generation, const QString &artist, const QString &title)
{
    if (generation != m_generation || (artist == m_artist && title == m_title))
        return;
    m_artist = artist;
    m_title = title;
    Q_EMIT metadataChanged();
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
        const auto target = m_urls.value(id);
        if (target.url.isEmpty()) {
            socket->write("HTTP/1.0 404 Not Found\r\n\r\n");
            socket->disconnectFromHost();
            return;
        }
        startSession(socket, target);
    });
}

void StreamProxy::startSession(QTcpSocket *socket, const Target &target)
{
    const auto &upstream = target.url;
    const int generation = target.generation;
    QNetworkRequest req(upstream);
    req.setHeader(QNetworkRequest::UserAgentHeader, u"KRadio/0.1"_s);
    req.setRawHeader("Icy-MetaData", "1");
    req.setTransferTimeout(0); // live streams never finish
    auto *reply = m_nam.get(req);

    struct Session {
        bool headerSent = false;
        bool gotMetadata = false;
        OggRebaser ogg;
        std::optional<IcyStripper> icy;
        // Ogg data is held until it contains CushionSeconds of audio. With a
        // server burst that's instant; without one (common for FLAC) playback
        // starts as soon as there's enough to ride out network jitter.
        bool live = false;
        QByteArray pending;

        // Song boundaries found while processing one chunk, in output order.
        struct Event {
            qsizetype offset;
            bool boundary;
            QString artist, title;
        };
        QList<Event> icyEvents, oggEvents;
        std::optional<TrackRecorder> recorder;
        QString extension; // from Content-Type; Ogg streams know their own
    };
    auto session = std::make_shared<Session>();

    // Callbacks stored inside the session must not own it (a shared_ptr
    // cycle would keep it, and its half-recorded song, alive forever).
    Session *self = session.get();
    auto report = [this, generation, self](const QString &artist, const QString &title) {
        self->gotMetadata = true;
        setMetadata(generation, artist, title);
    };
    session->ogg.onTags = [self, report](const QString &artist, const QString &title) {
        report(artist, title);
        self->oggEvents.append({-1, false, artist, title}); // belongs to the latest boundary
    };
    session->ogg.onStreamStart = [self](qsizetype offset) {
        self->oggEvents.append({offset, true, {}, {}});
    };

    connect(reply, &QNetworkReply::redirected, reply, &QNetworkReply::redirectAllowed);
    connect(socket, &QTcpSocket::disconnected, reply, [reply] {
        reply->abort();
        reply->deleteLater();
    });

    connect(reply, &QNetworkReply::metaDataChanged, socket, [=, this] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // Skip redirect hops; answer once for the final response. Errors are
        // left unanswered so the finished handler hands the player the
        // original URL, which then reports the error itself.
        if (status / 100 == 3 || status >= 400 || session->headerSent)
            return;
        session->headerSent = true;

        if (const int metaInt = reply->rawHeader("icy-metaint").toInt(); metaInt > 0) {
            session->icy.emplace(metaInt);
            session->icy->onTitle = [report, self](const QString &streamTitle, qsizetype offset) {
                const auto [artist, title] = splitStreamTitle(streamTitle);
                report(artist, title);
                self->icyEvents.append({offset, true, artist, title});
            };
        }

        const auto type = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        session->extension = type.contains("aac") ? u"aac"_s : type.contains("mpeg") ? u"mp3"_s : QString();
        socket->write("HTTP/1.0 200 OK\r\nContent-Type: " + (type.isEmpty() ? "application/octet-stream" : type)
                      + "\r\nCache-Control: no-cache\r\n\r\n");

        // Some Icecast mounts (often FLAC) carry no metadata at all; the
        // server's status page may still know what's playing.
        const bool icecast = reply->hasRawHeader("icy-name") || reply->rawHeader("Server").contains("Icecast");
        if (icecast) {
            auto *timer = new QTimer(socket);
            timer->setInterval(20000);
            auto poll = [this, session, streamUrl = reply->url(), generation] {
                if (!session->gotMetadata)
                    pollIcecastStatus(streamUrl, generation);
            };
            connect(timer, &QTimer::timeout, this, poll);
            QTimer::singleShot(4000, timer, [timer, poll] {
                poll();
                timer->start();
            });
        }
    });

    connect(reply, &QNetworkReply::readyRead, socket, [this, socket, reply, session, stationName = target.station] {
        auto data = reply->readAll();
        if (!session->headerSent)
            return; // an error page or redirect body, not audio
        session->icyEvents.clear();
        session->oggEvents.clear();
        if (session->icy)
            data = session->icy->process(data);
        data = session->ogg.process(data);

        // Ogg marks songs itself (and rewrites the bytes, so ICY offsets
        // wouldn't line up); other formats rely on ICY title changes.
        const bool isOgg = session->ogg.isOgg();
        const auto extension = isOgg ? session->ogg.extension() : session->extension;
        if (!session->recorder && !extension.isEmpty())
            session->recorder.emplace(&m_recordings, stationName, extension);
        if (auto &rec = session->recorder) {
            qsizetype pos = 0;
            for (const auto &e : std::as_const(isOgg ? session->oggEvents : session->icyEvents)) {
                if (e.offset >= 0) {
                    const auto cut = qBound(pos, e.offset, data.size());
                    rec->write(data.sliced(pos, cut - pos));
                    pos = cut;
                }
                if (e.boundary)
                    rec->boundary();
                if (!e.title.isEmpty() || !e.artist.isEmpty())
                    rec->setSong(e.artist, e.title);
            }
            rec->write(data.sliced(pos));
        }
        if (session->live) {
            socket->write(data);
            return;
        }
        session->pending += data;
        const auto &ogg = session->ogg;
        const auto seconds = ogg.emittedSeconds();
        // Non-Ogg, or an Ogg codec we can't time: no cushion, play right away.
        const bool cushioned = ogg.isDecided()
            && (!ogg.isOgg() || (seconds ? *seconds >= CushionSeconds : session->pending.size() > 256 * 1024));
        if (cushioned) {
            session->live = true;
            socket->write(std::exchange(session->pending, {}));
        }
    });

    connect(reply, &QNetworkReply::finished, socket, [socket, reply, upstream, session] {
        if (!session->headerSent) {
            // We couldn't talk to it (an HTTP error, or e.g. a SHOUTcast v1
            // "ICY 200 OK" reply): hand the player the original URL instead.
            socket->write("HTTP/1.0 302 Found\r\nLocation: " + upstream.toEncoded() + "\r\n\r\n");
        }
        socket->disconnectFromHost();
    });
}

void StreamProxy::pollIcecastStatus(const QUrl &streamUrl, int generation)
{
    if (generation != m_generation)
        return;

    QUrl statusUrl = streamUrl;
    const auto path = streamUrl.path();
    const auto mount = path.sliced(path.lastIndexOf(u'/') + 1);
    statusUrl.setPath(path.first(path.lastIndexOf(u'/') + 1) + u"status-json.xsl"_s);
    statusUrl.setQuery(QString());

    QNetworkRequest req(statusUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, u"KRadio/0.1"_s);
    req.setTransferTimeout(10000);
    auto *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mount, generation] {
        reply->deleteLater();
        const auto icestats = QJsonDocument::fromJson(reply->readAll())["icestats"_L1]["source"_L1];
        const auto sources = icestats.isArray() ? icestats.toArray() : QJsonArray{icestats};

        // Prefer our own mount. Otherwise use the mounts whose names share
        // the longest prefix with ours: "classic_opus" matches "classic", not
        // the same server's "jazz", which is a different station.
        auto commonPrefix = [&mount](const QString &other) {
            qsizetype n = 0;
            while (n < mount.size() && n < other.size() && mount[n] == other[n])
                ++n;
            return n;
        };
        QHash<std::pair<QString, QString>, int> votes;
        qsizetype bestPrefix = 1; // share at least one character
        for (const auto &v : sources) {
            const auto s = v.toObject();
            const std::pair<QString, QString> song{s["artist"_L1].toString(), s["title"_L1].toString()};
            if (song.second.isEmpty())
                continue;
            const auto path = QUrl(s["listenurl"_L1].toString()).path();
            const auto other = path.sliced(path.lastIndexOf(u'/') + 1);
            if (other == mount) {
                setMetadata(generation, song.first, song.second);
                return;
            }
            const auto prefix = commonPrefix(other);
            if (prefix > bestPrefix) {
                bestPrefix = prefix;
                votes.clear();
            }
            if (prefix == bestPrefix)
                ++votes[song];
        }
        if (votes.isEmpty())
            return;
        std::pair<QString, QString> best;
        int bestVotes = 0;
        for (const auto &[song, n] : votes.asKeyValueRange()) {
            if (n > bestVotes)
                best = song, bestVotes = n;
        }
        const auto &[artist, title] = best;
        // Icecast puts the whole "Artist - Title" in title when there's no artist.
        if (artist.isEmpty()) {
            const auto [a, t] = splitStreamTitle(title);
            setMetadata(generation, a, t);
        } else {
            setMetadata(generation, artist, title);
        }
    });
}
