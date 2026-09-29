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
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit RadioBrowser(QObject *parent = nullptr);

    QVariantList stations() const { return m_stations; }
    bool busy() const { return m_reply != nullptr; }
    QString error() const { return m_error; }

    // Empty query lists the most popular stations.
    Q_INVOKABLE void search(const QString &query);
    // Reports a play to radio-browser, as its API guidelines ask.
    Q_INVOKABLE void countClick(const QString &stationUuid);

Q_SIGNALS:
    void stationsChanged();
    void busyChanged();
    void errorChanged();

private:
    void resolveServer();
    QNetworkRequest request(const QString &path, const QUrlQuery &query = {}) const;
    void setError(const QString &error);

    QNetworkAccessManager m_nam;
    QUrl m_server;
    QString m_pendingQuery;
    bool m_hasPending = false;
    QNetworkReply *m_reply = nullptr;
    QVariantList m_stations;
    QString m_error;
};
