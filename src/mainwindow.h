// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mpris.h"
#include "radiobrowser.h"
#include "stationicons.h"
#include "streamproxy.h"
#include <QAudioOutput>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QSettings>
class StationDelegate;
class SongDelegate;
class QLineEdit;
class QTabBar;
class QListWidget;
class QLabel;
class QPushButton;
class QComboBox;
class QSlider;
class MainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    void reload();
    void rebuild();
    void play(const QVariantMap &station, bool newQueue = false);
    void resume();
    void stop();
    void toggle();
    void step(int delta);
    void favorite(const QVariantMap &station);
    bool isFavorite(const QVariantMap &station) const;
    void updatePlayer();
    void metadataChanged();
    void save();
    void contextMenu(const QPoint &point);
    QString songText(const QVariantMap &song) const;
    RadioBrowser m_browser;
    StreamProxy m_proxy;
    Mpris m_mpris;
    QAudioOutput m_audio;
    QMediaPlayer m_player;
    QSettings m_settings;
    StationIcons m_icons;
    QVariantList m_favorites, m_recent, m_history, m_queue, m_visible;
    QVariantMap m_current;
    bool m_actionClick = false;
    QString m_query, m_tag, m_lastSong;
    StationDelegate *m_stationDelegate;
    SongDelegate *m_songDelegate;
    QLineEdit *m_search;
    QTabBar *m_tabs;
    QListWidget *m_list;
    QLabel *m_message, *m_title, *m_detail;
    QWidget *m_bar;
    QPushButton *m_play, *m_favorite, *m_previous, *m_next, *m_mute, *m_clear, *m_sortAction;
    QComboBox *m_sort;
    QSlider *m_volume;
};
