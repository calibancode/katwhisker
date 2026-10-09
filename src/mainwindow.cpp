// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "aboutdialog.h"
#include "actionlistwidget.h"
#include "playerbar.h"
#include "songdelegate.h"
#include "stationdelegate.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMediaMetaData>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    auto load = [this](const char *key) {
        return QJsonDocument::fromJson(m_settings.value(key).toByteArray()).array().toVariantList();
    };
    m_favorites = load("favoritesJson");
    m_recent = load("recentJson");
    m_history = load("historyJson");
    m_queue = load("queueJson");
    m_current =
        QJsonDocument::fromJson(m_settings.value(QStringLiteral("currentJson")).toByteArray()).object().toVariantMap();
    m_query = m_settings.value(QStringLiteral("query")).toString();
    m_tag = m_settings.value(QStringLiteral("tag")).toString();
    auto *central = new QWidget(this);
    central->setAutoFillBackground(true);
    central->setBackgroundRole(QPalette::Base);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *header = new QWidget;
    header->setAutoFillBackground(true);
    header->setBackgroundRole(QPalette::Window);
    auto *toolbar = new QHBoxLayout(header);
    toolbar->setContentsMargins(6, 6, 6, 6);
    toolbar->setSpacing(4);
    m_search = new QLineEdit;
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumHeight(32);
    m_search->addAction(QIcon::fromTheme(QStringLiteral("edit-find")), QLineEdit::LeadingPosition);
    m_sort = new QComboBox;
    m_sort->addItems({i18n("Most Popular"), i18n("Trending"), i18n("Top Voted")});
    m_sort->setCurrentIndex(qBound(0, m_settings.value(QStringLiteral("order"), 0).toInt(), 2));
    auto iconButton = [](const QString &icon, const QString &text) {
        auto *button = new QPushButton(QIcon::fromTheme(icon), QString());
        button->setFlat(true);
        button->setFixedSize(32, 32);
        button->setIconSize(QSize(20, 20));
        button->setToolTip(text);
        button->setAccessibleName(text);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral("QPushButton::menu-indicator { image: none; width: 0px; }"));
        return button;
    };
    m_clear = iconButton(QStringLiteral("edit-clear-history"), i18n("Clear History"));
    m_clear->setObjectName(QStringLiteral("clearHistoryButton"));
    m_sortAction = iconButton(QStringLiteral("view-sort"), i18n("Sort"));
    connect(m_sortAction, &QPushButton::clicked, this, [this] {
        QMenu menu(this);
        for (int i = 0; i < m_sort->count(); ++i) {
            auto *action = menu.addAction(m_sort->itemText(i));
            action->setCheckable(true);
            action->setChecked(i == m_sort->currentIndex());
            connect(action, &QAction::triggered, this, [this, i] { m_sort->setCurrentIndex(i); });
        }
        menu.exec(m_sortAction->mapToGlobal(QPoint(0, m_sortAction->height())));
    });
    m_sort->setParent(header);
    m_sort->hide();
    auto *menuButton = iconButton(QStringLiteral("application-menu"), i18n("Menu"));
    auto *menu = new QMenu(menuButton);
    menu->addAction(i18n("About Katwhisker"), this, [this] {
        auto *dialog = new AboutDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    menuButton->setMenu(menu);
    toolbar->addWidget(m_search, 1);
    toolbar->addWidget(m_sortAction);
    toolbar->addWidget(m_clear);
    toolbar->addWidget(menuButton);
    layout->addWidget(header);
    m_tabs = new QTabBar;
    for (const auto &text : {i18n("Discover"), i18n("Favorites"), i18n("Recent"), i18n("Songs")})
        m_tabs->addTab(text);
    m_tabs->setExpanding(true);
    m_tabs->setDrawBase(false);
    m_tabs->setFixedHeight(32);
    m_tabs->setStyleSheet(
        QStringLiteral("QTabBar { background: palette(window); }"
                       "QTabBar::tab { padding: 6px 8px; border: 1px solid transparent; border-bottom: 0; }"
                       "QTabBar::tab:selected { background: palette(button); border: 1px solid palette(mid); "
                       "border-bottom: 0; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
                       "QTabBar::tab:hover:!selected { background: palette(alternate-base); }"));
    layout->addWidget(m_tabs);
    m_message = new QLabel;
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    layout->addWidget(m_message);
    auto *retry = new QPushButton(i18n("Refresh / Retry"));
    auto *all = new QPushButton(i18n("Show All"));
    auto *browse = new QHBoxLayout;
    browse->addWidget(retry);
    browse->addWidget(all);
    browse->addStretch();
    layout->addLayout(browse);
    auto updateBrowse = [this, retry, all] {
        const bool discover = m_tabs->currentIndex() == 0;
        retry->setVisible(discover && !m_browser.error().isEmpty());
        all->setVisible(discover && !m_tag.isEmpty());
    };
    connect(m_tabs, &QTabBar::currentChanged, this, updateBrowse);
    connect(&m_browser, &RadioBrowser::busyChanged, this, updateBrowse);
    connect(&m_browser, &RadioBrowser::errorChanged, this, updateBrowse);
    updateBrowse();
    connect(retry, &QPushButton::clicked, this, &MainWindow::reload);
    connect(all, &QPushButton::clicked, this, [this] {
        m_tag.clear();
        m_query.clear();
        m_search->clear();
        reload();
    });
    auto *stationList = new ActionListWidget;
    m_list = stationList;
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMouseTracking(true);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setIconSize(QSize(32, 32));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);
    auto *loadingMore = new QLabel(i18n("Loading stations…"));
    loadingMore->setAlignment(Qt::AlignCenter);
    loadingMore->setFixedHeight(30);
    layout->addWidget(loadingMore);
    auto updateMore = [this, loadingMore] {
        loadingMore->setVisible(m_tabs->currentIndex() == 0 && m_browser.busy() && !m_browser.stations().isEmpty());
    };
    connect(&m_browser, &RadioBrowser::stationsChanged, this, updateMore);
    connect(&m_browser, &RadioBrowser::busyChanged, this, updateMore);
    connect(m_tabs, &QTabBar::currentChanged, this, updateMore);
    connect(stationList, &ActionListWidget::endReached, this, [this] {
        if (m_tabs->currentIndex() == 0 && m_browser.canFetchMore() && !m_browser.busy() && m_browser.error().isEmpty())
            m_browser.fetchMore();
    });
    auto *bar = new PlayerBar;
    m_bar = bar;
    m_title = bar->title;
    m_title->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_title, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu menu(this);
        menu.addAction(i18n("Copy Song Title"), this, [this] { QApplication::clipboard()->setText(m_title->text()); });
        menu.exec(m_title->mapToGlobal(point));
    });
    m_detail = bar->detail;
    m_favorite = bar->favorite;
    m_previous = bar->previous;
    m_play = bar->play;
    m_next = bar->next;
    m_mute = bar->mute;
    m_volume = bar->volume;
    connect(bar, &PlayerBar::volumeScrolled, this, [this](int delta) {
        m_audio.setVolume(qBound(0.0f, m_audio.volume() + delta * 0.05f, 1.0f));
        m_audio.setMuted(false);
    });
    layout->addWidget(m_bar);
    setCentralWidget(central);
    m_stationDelegate = new StationDelegate(m_list);
    m_songDelegate = new SongDelegate(m_list);
    connect(m_stationDelegate, &StationDelegate::favoriteRequested, this, [this](const QVariantMap &station) {
        m_actionClick = true;
        QTimer::singleShot(0, this, [this, station] {
            favorite(station);
            m_actionClick = false;
        });
    });
    connect(m_songDelegate, &SongDelegate::removeRequested, this, [this](const QVariantMap &song) {
        m_actionClick = true;
        QTimer::singleShot(0, this, [this, song] {
            m_history.removeOne(song);
            rebuild();
            save();
            m_actionClick = false;
        });
    });
    connect(&m_icons, &StationIcons::loaded, this, [this](const QString &url) {
        if (url == m_current.value(QStringLiteral("favicon")).toString())
            updatePlayer();
        for (int row = 0; row < m_list->count(); ++row) {
            auto *item = m_list->item(row);
            if (item->data(Qt::UserRole).toMap().value(QStringLiteral("favicon")).toString() == url)
                item->setIcon(m_icons.icon(url));
        }
    });
    m_audio.setVolume(m_settings.value(QStringLiteral("volume"), 0.8).toFloat());
    m_audio.setMuted(m_settings.value(QStringLiteral("muted"), false).toBool());
    m_player.setAudioOutput(&m_audio);
    connect(m_play, &QPushButton::clicked, this, &MainWindow::toggle);
    connect(m_previous, &QPushButton::clicked, this, [this] { step(-1); });
    connect(m_next, &QPushButton::clicked, this, [this] { step(1); });
    connect(m_favorite, &QPushButton::clicked, this, [this] { favorite(m_current); });
    connect(m_mute, &QPushButton::clicked, this, [this] { m_audio.setMuted(!m_audio.isMuted()); });
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        m_audio.setVolume(value / 100.0f);
        m_audio.setMuted(false);
    });
    connect(&m_audio, &QAudioOutput::volumeChanged, this, &MainWindow::updatePlayer);
    connect(&m_audio, &QAudioOutput::mutedChanged, this, &MainWindow::updatePlayer);
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, &MainWindow::updatePlayer);
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, &MainWindow::updatePlayer);
    connect(&m_player, &QMediaPlayer::errorOccurred, this, &MainWindow::updatePlayer);
    connect(&m_player, &QMediaPlayer::metaDataChanged, this, &MainWindow::metadataChanged);
    connect(&m_proxy, &StreamProxy::metadataChanged, this, &MainWindow::metadataChanged);
    connect(&m_browser, &RadioBrowser::stationsChanged, this, &MainWindow::rebuild);
    connect(&m_browser, &RadioBrowser::busyChanged, this, &MainWindow::rebuild);
    connect(&m_browser, &RadioBrowser::errorChanged, this, &MainWindow::rebuild);
    connect(&m_browser, &RadioBrowser::voted, this, [this](const QString &, bool ok, const QString &message) {
        statusBar()->showMessage(ok ? i18n("Thanks for voting!") : message, 5000);
    });
    connect(m_tabs, &QTabBar::currentChanged, this, [this] {
        m_search->setText(m_tabs->currentIndex() == 0 ? m_query : QString());
        m_sort->blockSignals(true);
        m_sort->clear();
        if (m_tabs->currentIndex() == 0) {
            m_sort->addItems({i18n("Most Popular"), i18n("Trending"), i18n("Top Voted")});
            m_sort->setCurrentIndex(m_settings.value(QStringLiteral("order"), 0).toInt());
        } else {
            m_sort->addItems({i18n("Order Added"), i18n("Name")});
            m_sort->setCurrentIndex(m_settings.value(QStringLiteral("favoritesByName"), false).toBool() ? 1 : 0);
        }
        m_sort->blockSignals(false);
        rebuild();
    });
    auto *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(350);
    connect(m_search, &QLineEdit::textChanged, debounce, [debounce] { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, [this] {
        if (m_tabs->currentIndex() == 0) {
            m_query = m_search->text();
            reload();
        } else
            rebuild();
    });
    connect(m_sort, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_settings.setValue(m_tabs->currentIndex() == 0 ? "order" : "favoritesByName", index);
        if (m_tabs->currentIndex() == 0)
            reload();
        else
            rebuild();
    });
    connect(m_clear, &QPushButton::clicked, this, [this] {
        const int view = m_tabs->currentIndex();
        if (view != 2 && view != 3)
            return;
        if (view == 2)
            m_recent.clear();
        else
            m_history.clear();
        rebuild();
        save();
    });
    auto activateItem = [this](QListWidgetItem *item) {
        if (m_actionClick)
            return;
        auto data = item->data(Qt::UserRole).toMap();
        if (m_tabs->currentIndex() == 3)
            QApplication::clipboard()->setText(songText(data));
        else
            play(data, true);
    };
    connect(m_list, &QListWidget::itemClicked, this, activateItem);
    connect(m_list, &QListWidget::itemActivated, this, activateItem);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &MainWindow::contextMenu);
    connect(&m_mpris, &Mpris::raiseRequested, this, [this] {
        show();
        raise();
        activateWindow();
    });
    connect(&m_mpris, &Mpris::quitRequested, this, &QWidget::close);
    connect(&m_mpris, &Mpris::playRequested, this, &MainWindow::resume);
    connect(&m_mpris, &Mpris::pauseRequested, this, &MainWindow::stop);
    connect(&m_mpris, &Mpris::stopRequested, this, &MainWindow::stop);
    connect(&m_mpris, &Mpris::playPauseRequested, this, &MainWindow::toggle);
    connect(&m_mpris, &Mpris::nextRequested, this, [this] { step(1); });
    connect(&m_mpris, &Mpris::previousRequested, this, [this] { step(-1); });
    connect(&m_mpris, &Mpris::volumeRequested, this, [this](double value) {
        m_audio.setVolume(value);
        m_audio.setMuted(false);
    });
    auto shortcut = [this](const QKeySequence &key, auto fn) {
        auto *s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, fn);
    };
    shortcut(QKeySequence::Find, [this] { m_search->setFocus(); });
    shortcut(QKeySequence::Refresh, [this] {
        if (m_tabs->currentIndex() == 0)
            reload();
    });
    shortcut(QKeySequence(QStringLiteral("Ctrl+Right")), [this] { step(1); });
    shortcut(QKeySequence(QStringLiteral("Ctrl+Left")), [this] { step(-1); });
    auto *space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    auto *mute = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(space, &QShortcut::activated, this, &MainWindow::toggle);
    connect(mute, &QShortcut::activated, this, [this] { m_audio.setMuted(!m_audio.isMuted()); });
    connect(qApp, &QApplication::focusChanged, this, [space, mute](QWidget *, QWidget *focus) {
        const bool editing = qobject_cast<QLineEdit *>(focus) != nullptr;
        space->setEnabled(!editing);
        mute->setEnabled(!editing);
    });
    resize(m_settings.value(QStringLiteral("width"), 600).toInt(),
           m_settings.value(QStringLiteral("height"), 720).toInt());
    setMinimumSize(360, 360);
    if (m_settings.contains("geometry"))
        restoreGeometry(m_settings.value(QStringLiteral("geometry")).toByteArray());
    m_search->setText(m_query);
    m_tabs->setCurrentIndex(qBound(0, m_settings.value(QStringLiteral("view"), 0).toInt(), 3));
    updatePlayer();
    rebuild();
    reload();
    updateMore();
    if (m_settings.value(QStringLiteral("wasPlaying"), false).toBool())
        QTimer::singleShot(0, this, &MainWindow::resume);
}

MainWindow::~MainWindow()
{
    m_proxy.closeConnections();
    m_player.stop();
}
QString MainWindow::songText(const QVariantMap &song) const
{
    return song.value(QStringLiteral("artist")).toString().isEmpty()
               ? song.value(QStringLiteral("title")).toString()
               : i18nc("artist – song title", "%1 – %2", song.value(QStringLiteral("artist")).toString(),
                       song.value(QStringLiteral("title")).toString());
}
bool MainWindow::isFavorite(const QVariantMap &station) const
{
    return std::ranges::any_of(m_favorites, [&station](const QVariant &s) {
        return s.toMap().value(QStringLiteral("uuid")) == station.value(QStringLiteral("uuid"));
    });
}
void MainWindow::favorite(const QVariantMap &station)
{
    if (station.isEmpty())
        return;
    if (isFavorite(station))
        ::erase_if(m_favorites, [&station](const QVariant &s) {
            return s.toMap().value(QStringLiteral("uuid")) == station.value(QStringLiteral("uuid"));
        });
    else
        m_favorites.append(station);
    rebuild();
    updatePlayer();
    save();
}
void MainWindow::reload()
{
    m_list->verticalScrollBar()->setValue(0);
    m_browser.search(
        m_query, m_tag,
        static_cast<RadioBrowser::Order>(qBound(0, m_settings.value(QStringLiteral("order"), 0).toInt(), 2)));
}
void MainWindow::rebuild()
{
    const int view = m_tabs->currentIndex();
    m_list->setItemDelegate(view == 3 ? static_cast<QAbstractItemDelegate *>(m_songDelegate) : m_stationDelegate);
    m_visible = view == 0 ? m_browser.stations() : view == 1 ? m_favorites : view == 2 ? m_recent : m_history;
    if (view == 1 && m_settings.value(QStringLiteral("favoritesByName"), false).toBool())
        std::sort(m_visible.begin(), m_visible.end(), [](const QVariant &a, const QVariant &b) {
            return QString::localeAwareCompare(a.toMap().value(QStringLiteral("name")).toString(),
                                               b.toMap().value(QStringLiteral("name")).toString()) < 0;
        });
    const auto needle = m_search->text().trimmed();
    if (view != 0 && !needle.isEmpty())
        ::erase_if(m_visible, [&](const QVariant &v) {
            const auto s = v.toMap();
            return !QString(view == 3 ? songText(s) + s.value(QStringLiteral("station")).toString()
                                      : s.value(QStringLiteral("name")).toString() +
                                            s.value(QStringLiteral("tags")).toStringList().join(u' '))
                        .contains(needle, Qt::CaseInsensitive);
        });
    const auto scroll = m_list->verticalScrollBar()->value();
    m_list->clear();
    for (const auto &v : m_visible) {
        auto s = v.toMap();
        QString text;
        if (view == 3)
            text = songText(s) + u'\n' + s.value(QStringLiteral("station")).toString() + QStringLiteral("  ·  ") +
                   QLocale().toString(QDateTime::fromMSecsSinceEpoch(s.value(QStringLiteral("time")).toLongLong()),
                                      QLocale::ShortFormat);
        else {
            QStringList details;
            if (!s.value(QStringLiteral("country")).toString().isEmpty())
                details << s.value(QStringLiteral("country")).toString();
            if (s.value(QStringLiteral("bitrate")).toInt() > 0)
                details << i18n("%1 kbps", s.value(QStringLiteral("bitrate")).toInt());
            if (!s.value(QStringLiteral("codec")).toString().isEmpty())
                details << s.value(QStringLiteral("codec")).toString();
            details << i18np("%1 vote", "%1 votes", s.value(QStringLiteral("votes")).toInt());
            text = s.value(QStringLiteral("name")).toString() + u'\n' + details.join(QStringLiteral("  ·  ")) + u'\n' +
                   s.value(QStringLiteral("tags")).toStringList().join(QStringLiteral("  "));
        }
        auto *item = new QListWidgetItem(m_icons.icon(s.value(QStringLiteral("favicon")).toString()), text, m_list);
        item->setData(Qt::UserRole, s);
        item->setData(Qt::UserRole + 1, isFavorite(s));
        item->setData(Qt::UserRole + 4, view == 3       ? i18n("Remove")
                                        : isFavorite(s) ? i18n("Remove from Favorites")
                                                        : i18n("Add to Favorites"));
        item->setData(Qt::UserRole + 2,
                      view != 3 && s.value(QStringLiteral("uuid")) == m_current.value(QStringLiteral("uuid")));
        item->setData(Qt::UserRole + 3, view == 0 && !s.value(QStringLiteral("tags")).toStringList().isEmpty());
    }
    m_list->verticalScrollBar()->setValue(scroll);
    m_sortAction->setVisible(view < 2);
    m_clear->setVisible(view >= 2);
    m_clear->setEnabled(view == 2 ? !m_recent.isEmpty() : view == 3 && !m_history.isEmpty());
    m_search->setPlaceholderText(view == 0   ? i18n("Search stations…")
                                 : view == 1 ? i18n("Search favorites…")
                                 : view == 2 ? i18n("Search recent…")
                                             : i18n("Search songs…"));
    QString message;
    if (view == 0)
        message = m_browser.busy() && m_browser.stations().isEmpty() ? i18n("Loading stations…") : m_browser.error();
    if (view == 0 && !m_tag.isEmpty())
        message += i18n(" Showing stations tagged “%1”", m_tag);
    if (message.isEmpty() && m_visible.isEmpty())
        message = view == 0   ? i18n("No stations found")
                  : view == 1 ? i18n("No favorites yet")
                  : view == 2 ? i18n("Nothing played yet")
                              : i18n("No songs yet");
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
}
void MainWindow::play(const QVariantMap &station, bool newQueue)
{
    if (newQueue) {
        int i = m_visible.indexOf(station);
        m_queue = m_visible.mid(qMax(0, i - 100), 200);
    }
    if (station.value(QStringLiteral("uuid")) == m_current.value(QStringLiteral("uuid")) && m_player.isPlaying()) {
        updatePlayer();
        save();
        return;
    }
    m_proxy.closeConnections();
    m_player.stop();
    m_current = station;
    m_lastSong.clear();
    ::erase_if(m_recent, [&](const QVariant &v) {
        return v.toMap().value(QStringLiteral("uuid")) == station.value(QStringLiteral("uuid"));
    });
    m_recent.prepend(station);
    if (m_recent.size() > 50)
        m_recent.resize(50);
    resume();
    m_browser.countClick(station.value(QStringLiteral("uuid")).toString());
    rebuild();
    save();
}
void MainWindow::resume()
{
    if (m_current.isEmpty() || m_player.isPlaying())
        return;
    m_proxy.closeConnections();
    m_player.setSource(m_proxy.wrap(QUrl(m_current.value(QStringLiteral("url")).toString()),
                                    m_current.value(QStringLiteral("hls")).toBool()));
    m_player.play();
    updatePlayer();
}
void MainWindow::stop()
{
    m_proxy.closeConnections();
    m_player.stop();
    updatePlayer();
}
void MainWindow::toggle() { m_player.isPlaying() ? stop() : resume(); }
void MainWindow::step(int delta)
{
    if (m_queue.isEmpty())
        return;
    int i = -1;
    for (int j = 0; j < m_queue.size(); ++j)
        if (m_queue[j].toMap().value(QStringLiteral("uuid")) == m_current.value(QStringLiteral("uuid")))
            i = j;
    play(m_queue[(i < 0 ? 0 : (i + delta + m_queue.size()) % m_queue.size())].toMap());
}
void MainWindow::metadataChanged()
{
    QString title =
        m_proxy.title().isEmpty() ? m_player.metaData().stringValue(QMediaMetaData::Title).trimmed() : m_proxy.title();
    QVariantMap song{{QStringLiteral("title"), title},
                     {QStringLiteral("artist"), m_proxy.title().isEmpty() ? QString() : m_proxy.artist()},
                     {QStringLiteral("station"), m_current.value(QStringLiteral("name"))},
                     {QStringLiteral("favicon"), m_current.value(QStringLiteral("favicon"))},
                     {QStringLiteral("time"), QDateTime::currentMSecsSinceEpoch()}};
    const QString key = songText(song) + u'\n' + song.value(QStringLiteral("station")).toString();
    if (!title.isEmpty() && !m_current.isEmpty() && key != m_lastSong) {
        m_lastSong = key;
        if (m_history.isEmpty() || songText(m_history.first().toMap()) + u'\n' +
                                           m_history.first().toMap().value(QStringLiteral("station")).toString() !=
                                       key) {
            m_history.prepend(song);
            if (m_history.size() > 500)
                m_history.resize(500);
            save();
            if (m_tabs->currentIndex() == 3)
                rebuild();
        }
    }
    updatePlayer();
}
void MainWindow::updatePlayer()
{
    const bool playing = m_player.isPlaying();
    const auto title =
        m_proxy.title().isEmpty()
            ? m_player.metaData().stringValue(QMediaMetaData::Title).trimmed()
            : songText({{QStringLiteral("title"), m_proxy.title()}, {QStringLiteral("artist"), m_proxy.artist()}});
    m_bar->setVisible(!m_current.isEmpty());
    auto *bar = static_cast<PlayerBar *>(m_bar);
    bar->artwork->setPixmap(m_icons.icon(m_current.value(QStringLiteral("favicon")).toString()).pixmap(46, 46));
    setWindowTitle(m_current.isEmpty() ? i18n("Katwhisker") : m_current.value(QStringLiteral("name")).toString());
    m_title->setText(title.isEmpty() ? m_current.value(QStringLiteral("name")).toString() : title);
    m_title->setToolTip(m_title->text());
    m_detail->setText(m_player.error() != QMediaPlayer::NoError            ? m_player.errorString()
                      : m_player.mediaStatus() == QMediaPlayer::EndOfMedia ? i18n("Connection lost")
                      : m_player.mediaStatus() == QMediaPlayer::LoadingMedia ||
                              m_player.mediaStatus() == QMediaPlayer::StalledMedia
                          ? i18n("Connecting…")
                      : !title.isEmpty() ? m_current.value(QStringLiteral("name")).toString()
                      : playing          ? i18n("Live")
                                         : i18n("Stopped"));
    m_play->setToolTip(playing ? i18n("Stop") : i18n("Play"));
    m_play->setAccessibleName(m_play->toolTip());
    m_play->setIcon(
        QIcon::fromTheme(playing ? QStringLiteral("media-playback-stop") : QStringLiteral("media-playback-start"),
                         style()->standardIcon(playing ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay)));
    m_play->setText(QString());
    m_favorite->setToolTip(isFavorite(m_current) ? i18n("Remove from Favorites") : i18n("Add to Favorites"));
    m_favorite->setAccessibleName(m_favorite->toolTip());
    m_favorite->setIcon(QIcon::fromTheme(isFavorite(m_current) ? QStringLiteral("starred-symbolic")
                                                               : QStringLiteral("non-starred-symbolic")));
    m_favorite->setText(
        m_favorite->icon().isNull() ? (isFavorite(m_current) ? QStringLiteral("★") : QStringLiteral("☆")) : QString());
    m_previous->setEnabled(m_queue.size() > 1);
    m_next->setEnabled(m_queue.size() > 1);
    m_mute->setToolTip(m_audio.isMuted() ? i18n("Unmute") : i18n("Mute"));
    m_mute->setAccessibleName(m_mute->toolTip());
    const bool silent = m_audio.isMuted() || m_audio.volume() == 0;
    m_mute->setIcon(
        QIcon::fromTheme(silent ? QStringLiteral("audio-volume-muted") : QStringLiteral("audio-volume-high"),
                         style()->standardIcon(silent ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume)));
    m_mute->setText(QString());
    m_volume->blockSignals(true);
    m_volume->setValue(qRound(m_audio.volume() * 100));
    m_volume->blockSignals(false);
    m_mpris.setProperty("playing", playing);
    m_mpris.setProperty("canPlay", !m_current.isEmpty());
    m_mpris.setProperty("canGoNext", m_queue.size() > 1);
    m_mpris.setProperty("canGoPrevious", m_queue.size() > 1);
    m_mpris.setProperty("volume", m_audio.isMuted() ? 0.0 : m_audio.volume());
    m_mpris.setProperty("stationName", m_current.value(QStringLiteral("name")));
    m_mpris.setProperty("title", m_proxy.title().isEmpty() ? title : m_proxy.title());
    m_mpris.setProperty("artist", m_proxy.artist());
    m_mpris.setProperty("artUrl", m_current.value(QStringLiteral("favicon")));
    m_mpris.setProperty("trackId", m_current.value(QStringLiteral("uuid")));
}
void MainWindow::contextMenu(const QPoint &point)
{
    auto *item = m_list->itemAt(point);
    if (!item)
        return;
    const auto s = item->data(Qt::UserRole).toMap();
    QMenu menu(this);
    if (m_tabs->currentIndex() == 3) {
        menu.addAction(i18n("Copy Song Title"), this, [this, s] { QApplication::clipboard()->setText(songText(s)); });
        menu.addAction(i18n("Remove"), this, [this, s] {
            m_history.removeOne(s);
            rebuild();
            save();
        });
    } else {
        menu.addAction(i18n("Play"), this, [this, s] { play(s, true); });
        menu.addAction(isFavorite(s) ? i18n("Remove from Favorites") : i18n("Add to Favorites"), this,
                       [this, s] { favorite(s); });
        menu.addAction(i18n("Open Homepage"), this,
                       [s] { QDesktopServices::openUrl(QUrl(s.value(QStringLiteral("homepage")).toString())); })
            ->setEnabled(!s.value(QStringLiteral("homepage")).toString().isEmpty());
        menu.addAction(i18n("Copy Stream URL"), this,
                       [s] { QApplication::clipboard()->setText(s.value(QStringLiteral("url")).toString()); });
        menu.addAction(i18n("Vote for Station"), this,
                       [this, s] { m_browser.vote(s.value(QStringLiteral("uuid")).toString()); });
        auto *tags = menu.addMenu(i18n("Browse Tag"));
        for (const auto &tag : s.value(QStringLiteral("tags")).toStringList())
            tags->addAction(tag, this, [this, tag] {
                m_tag = tag;
                m_query.clear();
                m_tabs->setCurrentIndex(0);
                m_search->clear();
                reload();
            });
    }
    menu.exec(m_list->viewport()->mapToGlobal(point));
}
void MainWindow::save()
{
    auto store = [this](const char *key, const QVariantList &list) {
        m_settings.setValue(
            key, QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(list)).toJson(QJsonDocument::Compact)));
    };
    store("favoritesJson", m_favorites);
    store("recentJson", m_recent);
    store("historyJson", m_history);
    store("queueJson", m_queue);
    m_settings.setValue(
        "currentJson",
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(m_current)).toJson(QJsonDocument::Compact)));
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    save();
    m_settings.setValue("wasPlaying", m_player.isPlaying());
    m_settings.setValue("volume", m_audio.volume());
    m_settings.setValue("muted", m_audio.isMuted());
    m_settings.setValue("view", m_tabs->currentIndex());
    m_settings.setValue("query", m_query);
    m_settings.setValue("tag", m_tag);
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("width", width());
    m_settings.setValue("height", height());
    m_settings.sync();
    stop();
    QMainWindow::closeEvent(event);
}
