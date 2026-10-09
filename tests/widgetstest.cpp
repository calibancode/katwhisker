// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "aboutdialog.h"
#include "actionlistwidget.h"
#include "mainwindow.h"
#include "playerbar.h"
#include "rowpainting.h"
#include <KAboutData>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QTabBar>
#include <QTemporaryDir>
#include <QtTest>

// Wayland deliberately disallows QCursor::setPos (used by QTest::mouseMove).
// Deliver the same viewport event directly, also making offscreen runs deterministic.
static void movePointer(QWidget *widget, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(position), QPointF(widget->mapToGlobal(position)), Qt::NoButton,
                      Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

class WidgetsTest : public QObject
{
    Q_OBJECT
  private Q_SLOTS:
    void clearHistoryImmediately_data()
    {
        QTest::addColumn<int>("view");
        QTest::newRow("recent") << 2;
        QTest::newRow("songs") << 3;
    }
    void clearHistoryImmediately()
    {
        QFETCH(int, view);
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("katwhisker-test"));
        QCoreApplication::setApplicationName(QStringLiteral("clear-history"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        QSettings settings;
        const QVariantList entries{QVariantMap{{QStringLiteral("uuid"), QStringLiteral("one")},
                                               {QStringLiteral("name"), QStringLiteral("Visible")},
                                               {QStringLiteral("title"), QStringLiteral("Visible")}},
                                   QVariantMap{{QStringLiteral("uuid"), QStringLiteral("two")},
                                               {QStringLiteral("name"), QStringLiteral("Hidden")},
                                               {QStringLiteral("title"), QStringLiteral("Hidden")}}};
        const auto json = QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(entries)).toJson());
        for (const auto &key :
             {QStringLiteral("recentJson"), QStringLiteral("historyJson"), QStringLiteral("favoritesJson")})
            settings.setValue(key, json);
        settings.setValue(QStringLiteral("view"), view);
        settings.sync();
        MainWindow window;
        window.show();
        auto *list = window.findChild<QListWidget *>();
        auto *search = window.findChild<QLineEdit *>();
        auto *button = window.findChild<QPushButton *>(QStringLiteral("clearHistoryButton"));
        QVERIFY(list && search && button);
        search->setText(QStringLiteral("Visible"));
        QTRY_COMPARE(list->count(), 1);
        button->click();
        QVERIFY(window.findChild<QDialog *>(QStringLiteral("clearHistoryConfirmation")) == nullptr);
        QCOMPARE(list->count(), 0);
        QVERIFY(!button->isEnabled());
        settings.sync();
        QCOMPARE(QJsonDocument::fromJson(settings.value(view == 2 ? "recentJson" : "historyJson").toByteArray())
                     .array()
                     .size(),
                 0);
        QCOMPARE(QJsonDocument::fromJson(settings.value(view == 2 ? "historyJson" : "recentJson").toByteArray())
                     .array()
                     .size(),
                 2);
        QCOMPARE(QJsonDocument::fromJson(settings.value(QStringLiteral("favoritesJson")).toByteArray()).array().size(),
                 2);
        window.close();
    }
    void scrollingLoadsMore()
    {
        ActionListWidget list;
        list.resize(300, 160);
        QSignalSpy reached(&list, &ActionListWidget::endReached);
        for (int i = 0; i < 30; ++i)
            list.addItem(QString::number(i));
        list.show();
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > 0);
        QTest::qWait(20);
        QCOMPARE(reached.count(), 0);
        list.verticalScrollBar()->setValue(list.verticalScrollBar()->maximum());
        QTRY_COMPARE(reached.count(), 1);
        const int position = list.verticalScrollBar()->value();
        const int oldMaximum = list.verticalScrollBar()->maximum();
        for (int i = 30; i < 60; ++i)
            list.addItem(QString::number(i));
        QTRY_VERIFY(list.verticalScrollBar()->maximum() > oldMaximum);
        QTest::qWait(20);
        QCOMPARE(list.verticalScrollBar()->value(), position);
        QCOMPARE(reached.count(), 1);
        list.verticalScrollBar()->setValue(list.verticalScrollBar()->maximum());
        QTRY_COMPARE(reached.count(), 2);
        list.close();
        ActionListWidget shortList;
        shortList.resize(300, 160);
        QSignalSpy shortReached(&shortList, &ActionListWidget::endReached);
        shortList.addItem(QStringLiteral("A short first page"));
        shortList.show();
        QTRY_VERIFY(shortReached.count() > 0);
    }
    void layoutAndAbout()
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        KAboutData about(QStringLiteral("katwhisker"), QStringLiteral("Katwhisker"), QStringLiteral("0.3.0"),
                         QStringLiteral("A tiny web radio player powered by radio-browser.info"),
                         KAboutLicense::GPL_V3);
        about.setCopyrightStatement(QStringLiteral("Copyright 2026 calibancode - lifetime regression fixture"));
        about.setHomepage(QStringLiteral("https://github.com/calibancode/katwhisker"));
        about.setBugAddress("https://github.com/calibancode/katwhisker/issues");
        about.addAuthor(QStringLiteral("calibancode"), QStringLiteral("Developer"));
        about.addCredit(QStringLiteral("radio-browser.info"), QStringLiteral("Station directory"), {},
                        QStringLiteral("https://www.radio-browser.info/"));
        KAboutData::setApplicationData(about);
        QCoreApplication::setOrganizationName(QStringLiteral("katwhisker-test"));
        QCoreApplication::setApplicationName(QStringLiteral("layout"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        const auto originalPalette = QApplication::palette();
        const auto originalTheme = QIcon::themeName();
        QIcon::setThemeName(QStringLiteral("breeze-dark"));
        QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/io.github.calibancode.katwhisker.svg")));
        QPalette dark;
        dark.setColor(QPalette::Window, QColor(32, 35, 38));
        dark.setColor(QPalette::Base, QColor(27, 28, 28));
        dark.setColor(QPalette::AlternateBase, QColor(35, 39, 41));
        dark.setColor(QPalette::Button, QColor(32, 35, 38));
        dark.setColor(QPalette::Mid, QColor(64, 69, 71));
        for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            dark.setColor(role, QColor(239, 240, 241));
        dark.setColor(QPalette::Highlight, QColor(48, 75, 49));
        dark.setColor(QPalette::HighlightedText, Qt::white);
        dark.setColor(QPalette::Link, QColor(122, 187, 220));
        QApplication::setPalette(dark);
        QSettings settings;
        QVariantList stations;
        for (const auto &name : {QStringLiteral("Lapfox Radio (FLAC)"), QStringLiteral("Proteus Radio"),
                                 QStringLiteral("Classic Vinyl HD")}) {
            stations.append(
                QVariantMap{{QStringLiteral("uuid"), name.left(3)},
                            {QStringLiteral("name"), name},
                            {QStringLiteral("country"), QStringLiteral("US")},
                            {QStringLiteral("codec"), QStringLiteral("MP3")},
                            {QStringLiteral("bitrate"), 320},
                            {QStringLiteral("votes"), 342},
                            {QStringLiteral("tags"),
                             QStringList{QStringLiteral("progressive rock"), QStringLiteral("progressive-metal"),
                                         QStringLiteral("a very long station tag to test text elision")}}});
        }
        settings.setValue(QStringLiteral("favoritesJson"),
                          QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(stations)).toJson()));
        settings.setValue(
            QStringLiteral("currentJson"),
            QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(stations.first().toMap())).toJson()));
        settings.setValue(QStringLiteral("queueJson"), settings.value(QStringLiteral("favoritesJson")));
        settings.setValue(QStringLiteral("view"), 1);
        settings.sync();
        MainWindow window;
        window.resize(468, 648);
        window.show();
        QTest::qWait(100);
        auto *list = window.findChild<QListWidget *>();
        QVERIFY(list);
        QCOMPARE(list->count(), 3);
        QCOMPARE(list->horizontalScrollBar()->maximum(), 0);
        QVERIFY(list->visualItemRect(list->item(0)).height() <= 50);
        auto *bar = window.findChild<PlayerBar *>();
        QVERIFY(bar);
        QCOMPARE(bar->height(), 62);
        QVERIFY(bar->volume->isVisible());
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-dark.png"));
        movePointer(list->viewport(), list->visualItemRect(list->item(1)).center());
        QTest::qWait(20);
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-row-hover.png"));
        const auto actionRect = RowPainting::actionRect(list->visualItemRect(list->item(0)));
        movePointer(list->viewport(), list->visualItemRect(list->item(0)).center());
        QTest::qWait(20);
        const auto sample = QPoint(actionRect.left() + 2, actionRect.center().y());
        const auto beforeHover = list->viewport()->grab().toImage().pixelColor(sample);
        movePointer(list->viewport(), actionRect.center());
        QTest::qWait(20);
        QCOMPARE(list->viewport()->cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(list->viewport()->grab().toImage().pixelColor(sample) != beforeHover);
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-favorite-hover.png"));

        window.resize(360, 600);
        QTest::qWait(50);
        QCOMPARE(window.width(), 360);
        QVERIFY(!bar->volume->isVisible());
        QCOMPARE(list->horizontalScrollBar()->maximum(), 0);
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-narrow.png"));
        AboutDialog dialog(&window);
        dialog.show();
        QTest::qWait(50);
        dialog.grab().save(QStringLiteral("/tmp/katwhisker-widgets-about.png"));
        QVERIFY(dialog.findChildren<QLabel *>().size() >= 12);
        QVERIFY(dialog.findChildren<QDialogButtonBox *>().isEmpty());
        auto *licenseButton = dialog.findChild<QPushButton *>(QStringLiteral("licenseButton"));
        QVERIFY(licenseButton);
        QTest::mouseClick(licenseButton, Qt::LeftButton);
        auto *licenseText = dialog.findChild<QPlainTextEdit *>();
        QVERIFY(licenseText);
        QVERIFY(licenseText->toPlainText().contains(QStringLiteral("GNU GENERAL PUBLIC LICENSE")));
        QCOMPARE(licenseText->toPlainText(), KAboutData::applicationData().licenses().first().text());
        // Repeated clicks reuse the viewer instead of accumulating windows.
        licenseButton->click();
        QCOMPARE(dialog.findChildren<QPlainTextEdit *>().size(), 1);
        QPointer<QDialog> licenseWindow = qobject_cast<QDialog *>(licenseText->window());
        QVERIFY(licenseWindow);
        licenseWindow->close();
        QTRY_VERIFY(licenseWindow.isNull());
        QTest::mouseClick(licenseButton, Qt::LeftButton);
        licenseText = dialog.findChild<QPlainTextEdit *>();
        QVERIFY(licenseText);
        QTest::keyClick(licenseText->window(), Qt::Key_Escape);
        QTRY_VERIFY(dialog.findChild<QPlainTextEdit *>() == nullptr);
        dialog.close();
        QApplication::setPalette(originalPalette);
        QIcon::setThemeName(QStringLiteral("breeze"));
        window.resize(468, 648);
        QTest::qWait(50);
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-light.png"));
        window.close();
        QIcon::setThemeName(originalTheme);
    }
    void savedDataAndActions()
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("katwhisker-test"));
        QCoreApplication::setApplicationName(QStringLiteral("widgets"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        QSettings settings;
        const QVariantMap station{{QStringLiteral("uuid"), QStringLiteral("fixture")},
                                  {QStringLiteral("name"), QStringLiteral("Fixture Radio")},
                                  {QStringLiteral("url"), QStringLiteral("http://127.0.0.1:1/unused")},
                                  {QStringLiteral("tags"), QStringList{QStringLiteral("jazz")}}};
        const QVariantMap song{{QStringLiteral("title"), QStringLiteral("Fixture Song")},
                               {QStringLiteral("artist"), QStringLiteral("Fixture Artist")},
                               {QStringLiteral("station"), QStringLiteral("Fixture Radio")},
                               {QStringLiteral("time"), 1000}};
        auto json = [](const QVariantMap &data) {
            return QString::fromUtf8(QJsonDocument(QJsonArray{QJsonObject::fromVariantMap(data)}).toJson());
        };
        settings.setValue(QStringLiteral("favoritesJson"), json(station));
        settings.setValue(QStringLiteral("historyJson"), json(song));
        settings.setValue(QStringLiteral("currentJson"),
                          QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(station)).toJson()));
        settings.setValue(QStringLiteral("view"), 1);
        settings.setValue(QStringLiteral("volume"), 0.42);
        settings.sync();
        MainWindow window;
        window.show();
        auto *tabs = window.findChild<QTabBar *>();
        auto *list = window.findChild<QListWidget *>();
        auto *search = window.findChild<QLineEdit *>();
        QVERIFY(tabs && list && search);
        QCOMPARE(tabs->currentIndex(), 1);
        QCOMPARE(list->count(), 1);
        QVERIFY(list->item(0)->text().contains(QStringLiteral("Fixture Radio")));
        search->setFocus();
        QTest::keyClicks(search, QStringLiteral("some music"));
        QCOMPARE(search->text(), QStringLiteral("some music"));
        search->setText(QStringLiteral("missing"));
        QTRY_COMPARE(list->count(), 0);
        search->clear();
        QTRY_COMPARE(list->count(), 1);
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-favorites.png"));
        const auto row = list->visualItemRect(list->item(0));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.right() - 20, row.center().y()));
        QTRY_COMPARE(list->count(), 0);
        tabs->setCurrentIndex(3);
        QCOMPARE(list->count(), 1);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(0)).center());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Fixture Artist – Fixture Song"));
        const auto songRow = list->visualItemRect(list->item(0));
        movePointer(list->viewport(), songRow.center());
        movePointer(list->viewport(), RowPainting::actionRect(songRow).center());
        QTest::qWait(20);
        QCOMPARE(list->viewport()->cursor().shape(), Qt::PointingHandCursor);
        window.grab().save(QStringLiteral("/tmp/katwhisker-widgets-delete-hover.png"));

        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          QPoint(songRow.right() - 20, songRow.center().y()));
        QTRY_COMPARE(list->count(), 0);
        if (QDBusConnection::sessionBus().isConnected()) {
            QDBusInterface player(
                QStringLiteral("org.mpris.MediaPlayer2.katwhisker.instance%1").arg(QCoreApplication::applicationPid()),
                QStringLiteral("/org/mpris/MediaPlayer2"), QStringLiteral("org.mpris.MediaPlayer2.Player"));
            QVERIFY(player.isValid());
            QCOMPARE(player.property("PlaybackStatus").toString(), QStringLiteral("Stopped"));
            QVERIFY(player.property("CanPlay").toBool());
            QVERIFY(player.setProperty("Volume", 0.25));
            QTRY_COMPARE(window.findChild<QSlider *>()->value(), 25);
        }
        window.close();
        settings.sync();
        QVERIFY(
            QJsonDocument::fromJson(settings.value(QStringLiteral("favoritesJson")).toByteArray()).array().isEmpty());
        QVERIFY(QJsonDocument::fromJson(settings.value(QStringLiteral("historyJson")).toByteArray()).array().isEmpty());
        QVERIFY(!settings.value(QStringLiteral("wasPlaying")).toBool());
        QCOMPARE(settings.value(QStringLiteral("volume")).toFloat(),
                 QDBusConnection::sessionBus().isConnected() ? 0.25f : 0.42f);
    }
};
QTEST_MAIN(WidgetsTest)
#include "widgetstest.moc"
