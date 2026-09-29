#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

using namespace Qt::Literals::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kradio");

    KAboutData about(u"kradio"_s, i18n("KRadio"), u"0.1.0"_s,
                     i18n("A tiny web radio player powered by radio-browser.info"),
                     KAboutLicense::GPL_V3);
    about.setOrganizationDomain("kde.org");
    about.setDesktopFileName(u"org.kde.kradio"_s);
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(u"radio"_s));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(u"org.kde.desktop"_s);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("org.kde.kradio", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
