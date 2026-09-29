#include "radiobrowser.h"

#include <QDnsLookup>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRandomGenerator>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr auto FallbackServer = "https://de1.api.radio-browser.info"_L1;
constexpr int PageSize = 100;

QVariantMap toStation(const QJsonObject &o)
{
    auto tags = o["tags"_L1].toString().split(u',', Qt::SkipEmptyParts);
    for (auto &t : tags)
        t = t.trimmed();
    tags.removeDuplicates();
    return {
        {u"uuid"_s, o["stationuuid"_L1].toString()},
        {u"name"_s, o["name"_L1].toString().trimmed()},
        {u"url"_s, o["url_resolved"_L1].toString()},
        {u"homepage"_s, o["homepage"_L1].toString()},
        {u"favicon"_s, o["favicon"_L1].toString()},
        {u"country"_s, o["countrycode"_L1].toString()},
        {u"tags"_s, QStringList(tags.mid(0, 6))},
        {u"codec"_s, o["codec"_L1].toString()},
        {u"bitrate"_s, o["bitrate"_L1].toInt()},
        {u"votes"_s, o["votes"_L1].toInt()},
        {u"hls"_s, o["hls"_L1].toInt() == 1},
    };
}
}

RadioBrowser::RadioBrowser(QObject *parent)
    : QObject(parent)
{
    m_nam.setAutoDeleteReplies(true);
    m_nam.setTransferTimeout(15000);
    resolveServer();
}

// radio-browser recommends picking a mirror from the SRV records.
void RadioBrowser::resolveServer()
{
    auto *dns = new QDnsLookup(QDnsLookup::SRV, u"_api._tcp.radio-browser.info"_s, this);
    connect(dns, &QDnsLookup::finished, this, [this, dns] {
        dns->deleteLater();
        const auto records = dns->serviceRecords();
        for (const auto &rec : records)
            m_mirrors.append(QUrl(u"https://"_s + rec.target()));
        if (dns->error() != QDnsLookup::NoError || m_mirrors.isEmpty())
            m_mirrors = {QUrl(FallbackServer)};
        m_server = m_mirrors.at(QRandomGenerator::global()->bounded(m_mirrors.size()));
        if (m_hasPending) {
            m_hasPending = false;
            load(false);
        }
    });
    dns->lookup();
}

QNetworkRequest RadioBrowser::request(const QString &path, const QUrlQuery &query) const
{
    QUrl url = m_server;
    url.setPath(path);
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, u"KRadio/0.1"_s);
    return req;
}

void RadioBrowser::search(const QString &name, const QString &tag, Order order)
{
    m_name = name.trimmed();
    m_tag = tag.trimmed();
    // QML passes a plain int restored from settings; don't trust its range.
    m_order = order >= Popular && order <= TopVoted ? order : Popular;
    if (m_server.isEmpty()) {
        m_hasPending = true;
        return;
    }
    load(false);
}

void RadioBrowser::fetchMore()
{
    if (m_canFetchMore && !m_reply && !m_server.isEmpty())
        load(true);
}

void RadioBrowser::load(bool append)
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply = nullptr;
    }

    static constexpr QLatin1StringView orders[] = {"clickcount"_L1, "clicktrend"_L1, "votes"_L1};

    QUrlQuery q;
    q.addQueryItem(u"hidebroken"_s, u"true"_s);
    q.addQueryItem(u"order"_s, orders[m_order]);
    q.addQueryItem(u"reverse"_s, u"true"_s);
    q.addQueryItem(u"limit"_s, QString::number(PageSize));
    q.addQueryItem(u"offset"_s, QString::number(append ? m_stations.size() : 0));
    if (!m_name.isEmpty())
        q.addQueryItem(u"name"_s, m_name);
    if (!m_tag.isEmpty()) {
        q.addQueryItem(u"tag"_s, m_tag);
        q.addQueryItem(u"tagExact"_s, u"true"_s);
    }

    m_reply = m_nam.get(request(u"/json/stations/search"_s, q));
    Q_EMIT busyChanged();

    connect(m_reply, &QNetworkReply::finished, this, [this, append, reply = m_reply] {
        m_reply = nullptr;
        Q_EMIT busyChanged();

        if (reply->error() != QNetworkReply::NoError) {
            // Mirrors do go down; try the next one before giving up. (Replies
            // we abort ourselves are disconnected first and never get here.)
            if (m_failovers < m_mirrors.size() - 1) {
                ++m_failovers;
                m_server = m_mirrors.at((m_mirrors.indexOf(m_server) + 1) % m_mirrors.size());
                load(append);
                return;
            }
            m_failovers = 0;
            setError(reply->errorString());
            return;
        }
        m_failovers = 0;
        setError({});

        const auto array = QJsonDocument::fromJson(reply->readAll()).array();
        if (!append)
            m_stations.clear();
        m_stations.reserve(m_stations.size() + array.size());
        for (const auto &v : array)
            m_stations.append(toStation(v.toObject()));
        m_canFetchMore = array.size() == PageSize;
        Q_EMIT stationsChanged();
    });
}

void RadioBrowser::countClick(const QString &stationUuid)
{
    if (!m_server.isEmpty() && !stationUuid.isEmpty())
        m_nam.get(request(u"/json/url/"_s + stationUuid));
}

void RadioBrowser::vote(const QString &stationUuid)
{
    if (m_server.isEmpty() || stationUuid.isEmpty())
        return;
    auto *reply = m_nam.get(request(u"/json/vote/"_s + stationUuid));
    connect(reply, &QNetworkReply::finished, this, [this, reply, stationUuid] {
        const auto o = QJsonDocument::fromJson(reply->readAll()).object();
        const bool ok = reply->error() == QNetworkReply::NoError && o["ok"_L1].toBool();
        Q_EMIT voted(stationUuid, ok, o["message"_L1].toString(reply->errorString()));
    });
}

void RadioBrowser::setError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorChanged();
    }
}
