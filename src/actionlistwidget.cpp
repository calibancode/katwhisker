// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "actionlistwidget.h"
#include "rowpainting.h"
#include <QHelpEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QToolTip>

ActionListWidget::ActionListWidget(QWidget *parent) : QListWidget(parent), m_endCheck(new QTimer(this))
{
    setMouseTracking(true);
    m_endCheck->setSingleShot(true);
    m_endCheck->setInterval(0);
    auto checkLater = [this] { m_endCheck->start(); };
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, checkLater);
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, checkLater);
    connect(model(), &QAbstractItemModel::rowsInserted, this, checkLater);
    connect(model(), &QAbstractItemModel::modelReset, this, checkLater);
    connect(m_endCheck, &QTimer::timeout, this, [this] {
        doItemsLayout();
        if (count() > 0 && verticalScrollBar()->value() >= verticalScrollBar()->maximum())
            Q_EMIT endReached();
    });
}

ActionListWidget::~ActionListWidget()
{
    // QListWidget tears down its model before QObject disconnects our slots.
    // Stop callbacks before that teardown can schedule another end check.
    disconnect(model(), nullptr, this, nullptr);
    disconnect(verticalScrollBar(), nullptr, this, nullptr);
    m_endCheck->stop();
}

bool ActionListWidget::viewportEvent(QEvent *event)
{
    const auto previous = indexAt(m_hoverPosition);
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Leave) {
        m_hoverPosition =
            event->type() == QEvent::Leave ? QPoint(-1, -1) : static_cast<QMouseEvent *>(event)->position().toPoint();
        setProperty("actionHoverPosition", m_hoverPosition);
        const auto current = indexAt(m_hoverPosition);
        const bool overAction =
            current.isValid() && RowPainting::actionRect(visualRect(current)).contains(m_hoverPosition);
        if (overAction)
            viewport()->setCursor(Qt::PointingHandCursor);
        else
            viewport()->unsetCursor();
        if (event->type() == QEvent::MouseButtonPress)
            setProperty("actionPressed", overAction && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton);
        else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Leave)
            setProperty("actionPressed", false);
        if (previous.isValid())
            viewport()->update(RowPainting::actionRect(visualRect(previous)));
        if (current.isValid())
            viewport()->update(RowPainting::actionRect(visualRect(current)));
    }
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const auto index = indexAt(help->pos());
        if (index.isValid() && RowPainting::actionRect(visualRect(index)).contains(help->pos())) {
            QToolTip::showText(help->globalPos(), index.data(Qt::UserRole + 4).toString(), viewport());
            return true;
        }
        QToolTip::hideText();
    }
    return QListWidget::viewportEvent(event);
}
