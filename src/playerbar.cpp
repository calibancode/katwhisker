// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "playerbar.h"
#include "elidedlabel.h"
#include <KLocalizedString>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>
PlayerBar::PlayerBar(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);
    setFixedHeight(62);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);
    artwork = new QLabel;
    artwork->setFixedSize(46, 46);
    layout->addWidget(artwork);
    auto *labels = new QVBoxLayout;
    labels->setSpacing(0);
    labels->setContentsMargins(4, 0, 4, 0);
    title = new ElidedLabel;
    title->setTextFormat(Qt::PlainText);
    auto titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    detail = new ElidedLabel;
    detail->setTextFormat(Qt::PlainText);
    auto smallFont = detail->font();
    smallFont.setPointSizeF(qMax(8.0, smallFont.pointSizeF() - 1));
    detail->setFont(smallFont);
    auto mutedPalette = detail->palette();
    auto mutedText = mutedPalette.color(QPalette::WindowText);
    mutedText.setAlpha(170);
    mutedPalette.setColor(QPalette::WindowText, mutedText);
    detail->setPalette(mutedPalette);
    labels->addStretch();
    labels->addWidget(title);
    labels->addWidget(detail);
    labels->addStretch();
    layout->addLayout(labels, 1);
    auto *controls = new QHBoxLayout;
    controls->setSpacing(0);
    auto button = [controls](const QString &text, const QString &icon) {
        auto *b = new QPushButton(QIcon::fromTheme(icon), text);
        b->setFlat(true);
        b->setFixedSize(30, 34);
        b->setIconSize(QSize(20, 20));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(text);
        b->setAccessibleName(text);
        controls->addWidget(b);
        return b;
    };
    favorite = button(i18n("Favorite"), QStringLiteral("non-starred-symbolic"));
    previous = button(i18n("Previous"), QStringLiteral("media-skip-backward"));
    play = button(i18n("Play"), QStringLiteral("media-playback-start"));
    next = button(i18n("Next"), QStringLiteral("media-skip-forward"));
    mute = button(i18n("Mute"), QStringLiteral("audio-volume-muted"));
    previous->setIcon(
        QIcon::fromTheme(QStringLiteral("media-skip-backward"), style()->standardIcon(QStyle::SP_MediaSkipBackward)));
    next->setIcon(
        QIcon::fromTheme(QStringLiteral("media-skip-forward"), style()->standardIcon(QStyle::SP_MediaSkipForward)));
    for (auto *b : {favorite, previous, play, next, mute})
        if (!b->icon().isNull())
            b->setText(QString());
    mute->installEventFilter(this);
    volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    volume->setAccessibleName(i18n("Volume"));
    volume->setFixedWidth(84);
    controls->addWidget(volume);
    layout->addLayout(controls);
}

bool PlayerBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == mute && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        Q_EMIT volumeScrolled(wheel->angleDelta().y() / 120);
        wheel->accept();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PlayerBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    volume->setVisible(width() >= 440);
}
void PlayerBar::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    QColor border = palette().color(QPalette::WindowText);
    border.setAlpha(32);
    painter.setPen(border);
    painter.drawLine(0, 0, width(), 0);
}
