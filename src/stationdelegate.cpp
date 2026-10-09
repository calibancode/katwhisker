// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "stationdelegate.h"
#include "rowpainting.h"
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

void StationDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const bool favorite = index.data(Qt::UserRole + 1).toBool();
    RowPainting::paint(painter, option, index,
                       favorite ? QStringLiteral("starred-symbolic") : QStringLiteral("non-starred-symbolic"),
                       favorite ? QStringLiteral("★") : QStringLiteral("☆"), false);
}
QSize StationDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const
{
    return QSize(0, index.data(Qt::UserRole + 3).toBool() ? 66 : 48);
}
bool StationDelegate::editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option,
                                  const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        const bool overAction = RowPainting::actionRect(option.rect).contains(mouse->position().toPoint());
        if (event->type() == QEvent::MouseButtonPress) {
            m_pressedAction = overAction ? index : QModelIndex();
            return overAction;
        }
        const bool clicked = overAction && m_pressedAction == index;
        m_pressedAction = QPersistentModelIndex();
        if (clicked) {
            Q_EMIT favoriteRequested(index.data(Qt::UserRole).toMap());
            return true;
        }
    }
    return false;
}
