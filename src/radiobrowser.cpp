#include "radiobrowser.h"

#include <QDnsLookup>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr auto FallbackServer = "https://de1.api.radio-browser.info"_L1;
constexpr int PageSize = 200;
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
        if (dns->error() == QDnsLookup::NoError && !records.isEmpty()) {
            const auto &rec = records.at(QRandomGenerator::global()->bounded(records.size()));
            m_server = QUrl(u"https://"_s + rec.target());
        } else {
            m_server = QUrl(FallbackServer);
        }
        if (m_hasPending) {
            m_hasPending = false;
            search(m_pendingQuery);
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

void RadioBrowser::search(const QString &query)
{
    if (m_server.isEmpty()) {
        m_pendingQuery = query;
        m_hasPending = true;
        return;
    }

    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply = nullptr;
    }

    QUrlQuery q;
    q.addQueryItem(u"hidebroken"_s, u"true"_s);
    q.addQueryItem(u"order"_s, u"clickcount"_s);
    q.addQueryItem(u"reverse"_s, u"true"_s);
    q.addQueryItem(u"limit"_s, QString::number(PageSize));
    if (!query.trimmed().isEmpty())
        q.addQueryItem(u"name"_s, query.trimmed());

    m_reply = m_nam.get(request(u"/json/stations/search"_s, q));
    Q_EMIT busyChanged();

    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] {
        m_reply = nullptr;
        Q_EMIT busyChanged();

        if (reply->error() != QNetworkReply::NoError) {
            setError(reply->errorString());
            return;
        }
        setError({});

        QVariantList list;
        const auto array = QJsonDocument::fromJson(reply->readAll()).array();
        list.reserve(array.size());
        for (const auto &v : array) {
            const auto o = v.toObject();
            list.append(QVariantMap{
                {u"uuid"_s, o["stationuuid"_L1].toString()},
                {u"name"_s, o["name"_L1].toString().trimmed()},
                {u"url"_s, o["url_resolved"_L1].toString()},
                {u"favicon"_s, o["favicon"_L1].toString()},
                {u"country"_s, o["countrycode"_L1].toString()},
                {u"tags"_s, o["tags"_L1].toString().replace(u',', u"  ·  "_s)},
                {u"codec"_s, o["codec"_L1].toString()},
                {u"bitrate"_s, o["bitrate"_L1].toInt()},
            });
        }
        m_stations = std::move(list);
        Q_EMIT stationsChanged();
    });
}

void RadioBrowser::countClick(const QString &stationUuid)
{
    if (!m_server.isEmpty() && !stationUuid.isEmpty())
        m_nam.get(request(u"/json/url/"_s + stationUuid));
}

void RadioBrowser::setError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorChanged();
    }
}
