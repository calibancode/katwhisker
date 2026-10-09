// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

#include "version.h"
#include "mainwindow.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

// Before the rename the app was "kradio"; carry its favorites and history over.
static void migrateOldSettings()
{
    const auto newFile = QSettings().fileName();
    const auto oldFile = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/kde.org/kradio.conf"_s;
    if (!QFile::exists(newFile) && QFile::exists(oldFile)) {
        QDir().mkpath(QFileInfo(newFile).path());
        QFile::copy(oldFile, newFile);
    }
}

int main(int argc, char *argv[])
{
    // This is an audio player: FFmpeg's video-device probing needlessly loads
    // CUDA/VAAPI drivers, including for encoding. Respect explicit overrides.
    for (const auto *name : {"QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "QT_FFMPEG_ENCODING_HW_DEVICE_TYPES"}) {
        if (!qEnvironmentVariableIsSet(name))
            qputenv(name, ",");
    }
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("katwhisker");

    KAboutData about(u"katwhisker"_s, i18n("Katwhisker"), QStringLiteral(KATWHISKER_VERSION_STRING),
                     i18n("A tiny web radio player powered by radio-browser.info"),
                     KAboutLicense::GPL_V3);
    about.setLicense(KAboutLicense::GPL_V3, KAboutLicense::OrLaterVersions);
    about.setOrganizationDomain("calibancode.github.io");
    about.setHomepage(u"https://github.com/calibancode/katwhisker"_s);
    about.setBugAddress("https://github.com/calibancode/katwhisker/issues");
    about.addAuthor(u"calibancode"_s, i18n("Developer"));
    about.addCredit(u"radio-browser.info"_s, i18n("Station directory"), {}, u"https://www.radio-browser.info/"_s);
    about.setDesktopFileName(u"io.github.calibancode.katwhisker"_s);
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(about.desktopFileName(), QIcon(u":/icons/"_s + about.desktopFileName() + u".svg"_s)));
    migrateOldSettings();

    // One instance only: two would play over each other and fight over
    // MPRIS and settings. Launching again raises the running window instead.
    KDBusService service(KDBusService::Unique);

    MainWindow window;
    QObject::connect(&service, &KDBusService::activateRequested, &window, [&window] {
        window.show();
        window.raise();
        window.activateWindow();
    });
    window.show();
    return app.exec();
}
