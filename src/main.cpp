#include "version.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
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
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("katwhisker");

    KAboutData about(u"katwhisker"_s, i18n("Katwhisker"), QStringLiteral(KATWHISKER_VERSION_STRING),
                     i18n("A tiny web radio player powered by radio-browser.info"),
                     KAboutLicense::GPL_V3);
    about.setLicense(KAboutLicense::GPL_V3, KAboutLicense::OrLaterVersions);
    about.setOrganizationDomain("calibancode.github.io");
    about.setHomepage(u"https://github.com/calibancode/katwhisker"_s);
    about.setBugAddress("https://github.com/calibancode/katwhisker/issues");
    about.setDesktopFileName(u"io.github.calibancode.katwhisker"_s);
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(about.desktopFileName(), QIcon::fromTheme(u"radio"_s)));
    migrateOldSettings();

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(u"org.kde.desktop"_s);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("io.github.calibancode.katwhisker", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
