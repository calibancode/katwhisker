// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>

// Thin client for the radio-browser.info API (https://api.radio-browser.info/).
class RadioBrowser : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList stations READ stations NOTIFY stationsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool canFetchMore READ canFetchMore NOTIFY stationsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Order { Popular, Trending, TopVoted };
    Q_ENUM(Order)

    explicit RadioBrowser(QObject *parent = nullptr);

    QVariantList stations() const { return m_stations; }
    bool busy() const { return m_reply != nullptr; }
    bool canFetchMore() const { return m_canFetchMore; }
    QString error() const { return m_error; }

    // Empty name and tag list the top stations for the given order.
    Q_INVOKABLE void search(const QString &name, const QString &tag, RadioBrowser::Order order);
    Q_INVOKABLE void fetchMore();
    // Reports a play to radio-browser, as its API guidelines ask.
    Q_INVOKABLE void countClick(const QString &stationUuid);
    Q_INVOKABLE void vote(const QString &stationUuid);

Q_SIGNALS:
    void stationsChanged();
    void busyChanged();
    void errorChanged();
    void voted(const QString &stationUuid, bool ok, const QString &message);

private:
    void resolveServer();
    void load(bool append);
    QNetworkRequest request(const QString &path, const QUrlQuery &query = {}) const;
    void setError(const QString &error);

    QNetworkAccessManager m_nam;
    QUrl m_server;
    QList<QUrl> m_mirrors;
    int m_failovers = 0;
    QString m_name;
    QString m_tag;
    Order m_order = Popular;
    bool m_hasPending = false;
    bool m_canFetchMore = false;
    QNetworkReply *m_reply = nullptr;
    QVariantList m_stations;
    QString m_error;
};
