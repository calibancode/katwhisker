#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// Exposes the player on D-Bus as MPRIS2, so media keys, the Plasma media
// applet and the lock screen can control it. QML feeds state in and reacts
// to the *Requested signals.
class Mpris : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool playing MEMBER m_playing NOTIFY stateChanged)
    Q_PROPERTY(bool canPlay MEMBER m_canPlay NOTIFY stateChanged)
    Q_PROPERTY(bool canGoNext MEMBER m_canGoNext NOTIFY stateChanged)
    Q_PROPERTY(bool canGoPrevious MEMBER m_canGoPrevious NOTIFY stateChanged)
    Q_PROPERTY(double volume MEMBER m_volume NOTIFY stateChanged)
    Q_PROPERTY(QString stationName MEMBER m_stationName NOTIFY stateChanged)
    Q_PROPERTY(QString title MEMBER m_title NOTIFY stateChanged)
    Q_PROPERTY(QString artist MEMBER m_artist NOTIFY stateChanged)
    Q_PROPERTY(QString artUrl MEMBER m_artUrl NOTIFY stateChanged)
    Q_PROPERTY(QString trackId MEMBER m_trackId NOTIFY stateChanged)

public:
    explicit Mpris(QObject *parent = nullptr);
    ~Mpris() override;

    QVariantMap metadata() const;
    QString playbackStatus() const;

Q_SIGNALS:
    void stateChanged();

    void raiseRequested();
    void quitRequested();
    void playRequested();
    void pauseRequested();
    void playPauseRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();
    void volumeRequested(double volume);

private:
    void notify();

    bool m_playing = false;
    bool m_canPlay = false;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
    double m_volume = 1.0;
    QString m_stationName;
    QString m_title;
    QString m_artist;
    QString m_artUrl;
    QString m_trackId;
    QString m_serviceName;
    QVariantMap m_lastPlayer;
    bool m_notifyQueued = false;
};

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRootAdaptor(Mpris *parent);

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const;
    QString desktopEntry() const;
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public Q_SLOTS:
    void Raise();
    void Quit();

private:
    Mpris *m_mpris;
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ rate)
    Q_PROPERTY(double MaximumRate READ rate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPlay)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    explicit MprisPlayerAdaptor(Mpris *parent);

    QString playbackStatus() const { return m_mpris->playbackStatus(); }
    double rate() const { return 1.0; }
    void setRate(double) { }
    QVariantMap metadata() const { return m_mpris->metadata(); }
    double volume() const { return m_mpris->property("volume").toDouble(); }
    void setVolume(double volume) { Q_EMIT m_mpris->volumeRequested(qBound(0.0, volume, 1.0)); }
    qlonglong position() const { return 0; }
    bool canGoNext() const { return m_mpris->property("canGoNext").toBool(); }
    bool canGoPrevious() const { return m_mpris->property("canGoPrevious").toBool(); }
    bool canPlay() const { return m_mpris->property("canPlay").toBool(); }
    bool canSeek() const { return false; }
    bool canControl() const { return true; }

public Q_SLOTS:
    void Next() { Q_EMIT m_mpris->nextRequested(); }
    void Previous() { Q_EMIT m_mpris->previousRequested(); }
    void Pause() { Q_EMIT m_mpris->pauseRequested(); }
    void PlayPause() { Q_EMIT m_mpris->playPauseRequested(); }
    void Stop() { Q_EMIT m_mpris->stopRequested(); }
    void Play() { Q_EMIT m_mpris->playRequested(); }
    void Seek(qlonglong) { }
    void SetPosition(const QDBusObjectPath &, qlonglong) { }
    void OpenUri(const QString &) { }

private:
    Mpris *m_mpris;
};
