// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "aboutdialog.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSysInfo>
#include <QVBoxLayout>
#include <kcoreaddons_version.h>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    const auto about = KAboutData::applicationData();
    setWindowTitle(i18n("About Katwhisker"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    auto *body = new QVBoxLayout(content);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);
    auto label = [](const QString &text, bool secondary = false) {
        auto *l = new QLabel(text);
        l->setTextFormat(Qt::PlainText);
        l->setWordWrap(true);
        if (secondary) {
            auto p = l->palette();
            auto c = p.color(QPalette::WindowText);
            c.setAlpha(175);
            p.setColor(QPalette::WindowText, c);
            l->setPalette(p);
            auto font = l->font();
            font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1));
            l->setFont(font);
        }
        return l;
    };
    auto card = [body]() {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("aboutCard"));
        frame->setAutoFillBackground(true);
        frame->setBackgroundRole(QPalette::Base);
        frame->setStyleSheet(QStringLiteral(
            "QFrame#aboutCard { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }"));
        auto *box = new QVBoxLayout(frame);
        box->setContentsMargins(14, 12, 14, 12);
        box->setSpacing(6);
        body->addWidget(frame);
        return box;
    };
    auto *identity = card();
    auto *heading = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(QApplication::windowIcon().pixmap(56, 56));
    icon->setFixedSize(56, 56);
    heading->addWidget(icon);
    heading->addSpacing(8);
    auto *description = new QVBoxLayout;
    auto *name = label(about.displayName() + u' ' + about.version());
    auto font = name->font();
    font.setPointSizeF(font.pointSizeF() + 3);
    name->setFont(font);
    description->addWidget(name);
    description->addWidget(label(about.shortDescription(), true));
    heading->addLayout(description, 1);
    identity->addLayout(heading);
    body->addWidget(label(i18n("License")));
    const auto licenses = about.licenses();
    const QString licenseName = licenses.isEmpty() ? i18n("License") : licenses.first().name();
    // KAboutLicense retains a pointer to its KAboutData. Materialize the text
    // while `about` is alive instead of keeping a license in the click handler.
    const QString licenseText = licenses.isEmpty() ? QString() : licenses.first().text();
    auto *license = new QPushButton(licenseName);
    license->setObjectName(QStringLiteral("licenseButton"));
    license->setAutoDefault(false);
    license->setEnabled(!licenseText.isEmpty());
    license->setMinimumHeight(44);
    license->setCursor(Qt::PointingHandCursor);
    license->setStyleSheet(QStringLiteral("QPushButton { text-align: left; padding: 0 14px; background: palette(base); "
                                          "border: 1px solid palette(mid); border-radius: 6px; }"
                                          "QPushButton:hover { background: palette(alternate-base); }"
                                          "QPushButton:focus { border-color: palette(highlight); }"));
    auto *licenseRow = new QHBoxLayout(license);
    licenseRow->setContentsMargins(14, 0, 14, 0);
    licenseRow->addStretch();
    auto *arrow = new QLabel(QStringLiteral("›"));
    arrow->setAttribute(Qt::WA_TransparentForMouseEvents);
    licenseRow->addWidget(arrow);
    body->addWidget(license);
    connect(license, &QPushButton::clicked, this, [this, licenseName, licenseText] {
        if (m_licenseDialog) {
            m_licenseDialog->show();
            m_licenseDialog->raise();
            m_licenseDialog->activateWindow();
            return;
        }
        auto *dialog = new QDialog(this);
        m_licenseDialog = dialog;
        dialog->setObjectName(QStringLiteral("licenseDialog"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(licenseName);
        dialog->resize(680, 540);
        auto *box = new QVBoxLayout(dialog);
        box->setContentsMargins(12, 12, 12, 12);
        auto *text = new QPlainTextEdit;
        text->setReadOnly(true);
        text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        text->setPlainText(licenseText);
        box->addWidget(text);
        dialog->show();
    });
    auto *links = new QHBoxLayout;
    auto link = [this, links](const QString &text, const QString &iconName, const QUrl &url) {
        auto *button = new QPushButton(QIcon::fromTheme(iconName), text);
        button->setMinimumHeight(44);
        button->setAutoDefault(false);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }"
            "QPushButton:hover { background: palette(alternate-base); }"
            "QPushButton:focus { border-color: palette(highlight); }"));
        links->addWidget(button);
        connect(button, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
    };
    link(i18n("Homepage"), QStringLiteral("internet-services"), QUrl(about.homepage()));
    link(i18n("Report a Bug"), QStringLiteral("tools-report-bug"), QUrl(about.bugAddress()));
    body->addLayout(links);
    auto *librariesHeading = new QHBoxLayout;
    librariesHeading->addWidget(label(i18n("Libraries in use")), 1);
    auto *copy = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy to Clipboard"));
    copy->setFlat(true);
    copy->setAutoDefault(false);
    copy->setCursor(Qt::PointingHandCursor);
    librariesHeading->addWidget(copy);
    body->addLayout(librariesHeading);
    const QString platform =
        QSysInfo::prettyProductName() + QStringLiteral(" (%1)").arg(QGuiApplication::platformName());
    const QString frameworks = i18n("KDE Frameworks %1", QStringLiteral(KCOREADDONS_VERSION_STRING));
    const QString qt =
        i18n("Qt %1 (built against %2)", QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR));
    auto *libraries = card();
    auto library = [libraries, label](const QString &title, const QString &detail, bool separator) {
        if (separator) {
            auto *line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            libraries->addWidget(line);
        }
        libraries->addWidget(label(title));
        libraries->addWidget(label(detail, true));
    };
    library(platform, i18n("Underlying platform."), false);
    library(frameworks, i18n("Libraries from the KDE community extending Qt."), true);
    library(qt, i18n("Cross-platform application development framework."), true);
    connect(copy, &QPushButton::clicked, this, [platform, frameworks, qt] {
        QApplication::clipboard()->setText(platform + u'\n' + frameworks + u'\n' + qt);
    });
    auto people = [&](const QString &title, const QList<KAboutPerson> &persons) {
        if (persons.isEmpty())
            return;
        body->addWidget(label(title));
        for (const auto &person : persons) {
            auto *box = card();
            auto *row = new QHBoxLayout;
            auto *avatar = new QLabel(person.name().left(1).toUpper());
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setFixedSize(36, 36);
            avatar->setStyleSheet(QStringLiteral(
                "border: 1px solid palette(highlight); border-radius: 18px; color: palette(highlight);"));
            row->addWidget(avatar);
            row->addSpacing(8);
            auto *text = new QVBoxLayout;
            text->addWidget(label(person.name()));
            text->addWidget(label(person.task(), true));
            row->addLayout(text, 1);
            if (!person.webAddress().isEmpty()) {
                auto *web = new QPushButton(QIcon::fromTheme(QStringLiteral("internet-services")), QString());
                web->setFlat(true);
                web->setAutoDefault(false);
                web->setFixedSize(32, 32);
                web->setToolTip(i18n("Homepage"));
                connect(web, &QPushButton::clicked, this,
                        [person] { QDesktopServices::openUrl(QUrl(person.webAddress())); });
                row->addWidget(web);
            }
            box->addLayout(row);
        }
    };
    people(i18n("Authors"), about.authors());
    people(i18n("Credits"), about.credits());
    body->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll);
    // Fit the complete page when the screen allows it, and scroll on small displays.
    const auto *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    const int availableHeight = screen ? screen->availableGeometry().height() * 0.9 : 900;
    const int desiredHeight =
        content->sizeHint().height() + layout->contentsMargins().top() + layout->contentsMargins().bottom();
    resize(550, qMin(availableHeight, desiredHeight));
}
