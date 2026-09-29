#include "mpris.h"

#include <KAboutData>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr auto ObjectPath = "/org/mpris/MediaPlayer2"_L1;
}

Mpris::Mpris(QObject *parent)
    : QObject(parent)
    , m_serviceName(u"org.mpris.MediaPlayer2.kradio.instance%1"_s.arg(QCoreApplication::applicationPid()))
{
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);

    auto bus = QDBusConnection::sessionBus();
    bus.registerObject(ObjectPath, this);
    bus.registerService(m_serviceName);

    connect(this, &Mpris::stateChanged, this, [this] {
        // Coalesce the burst of property updates QML sends in one frame.
        if (!m_notifyQueued) {
            m_notifyQueued = true;
            QTimer::singleShot(0, this, &Mpris::notify);
        }
    });
}

Mpris::~Mpris()
{
    auto bus = QDBusConnection::sessionBus();
    bus.unregisterService(m_serviceName);
    bus.unregisterObject(ObjectPath);
}

QString Mpris::playbackStatus() const
{
    return m_playing ? u"Playing"_s : u"Stopped"_s;
}

QVariantMap Mpris::metadata() const
{
    if (m_trackId.isEmpty())
        return {};
    // MPRIS track ids must be valid object paths.
    QString id = m_trackId;
    id.replace(u'-', u'_');
    QVariantMap map{
        {u"mpris:trackid"_s, QVariant::fromValue(QDBusObjectPath(u"/org/kde/kradio/station/"_s + id))},
        {u"xesam:title"_s, m_title.isEmpty() ? m_stationName : m_title},
        {u"xesam:album"_s, m_stationName},
    };
    if (!m_title.isEmpty())
        map[u"xesam:artist"_s] = QStringList{m_artist.isEmpty() ? m_stationName : m_artist};
    if (!m_artUrl.isEmpty())
        map[u"mpris:artUrl"_s] = m_artUrl;
    return map;
}

void Mpris::notify()
{
    m_notifyQueued = false;

    const QVariantMap player{
        {u"PlaybackStatus"_s, playbackStatus()},
        {u"Metadata"_s, metadata()},
        {u"Volume"_s, m_volume},
        {u"CanPlay"_s, m_canPlay},
        {u"CanPause"_s, m_canPlay},
        {u"CanGoNext"_s, m_canGoNext},
        {u"CanGoPrevious"_s, m_canGoPrevious},
    };

    QVariantMap changed;
    for (auto it = player.cbegin(); it != player.cend(); ++it) {
        if (m_lastPlayer.value(it.key()) != it.value())
            changed.insert(it.key(), it.value());
    }
    m_lastPlayer = player;
    if (changed.isEmpty())
        return;

    auto msg = QDBusMessage::createSignal(ObjectPath, u"org.freedesktop.DBus.Properties"_s, u"PropertiesChanged"_s);
    msg << u"org.mpris.MediaPlayer2.Player"_s << changed << QStringList{};
    QDBusConnection::sessionBus().send(msg);
}

MprisRootAdaptor::MprisRootAdaptor(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
}

QString MprisRootAdaptor::identity() const
{
    return KAboutData::applicationData().displayName();
}

QString MprisRootAdaptor::desktopEntry() const
{
    return KAboutData::applicationData().desktopFileName();
}

void MprisRootAdaptor::Raise()
{
    Q_EMIT m_mpris->raiseRequested();
}

void MprisRootAdaptor::Quit()
{
    Q_EMIT m_mpris->quitRequested();
}

MprisPlayerAdaptor::MprisPlayerAdaptor(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
}
