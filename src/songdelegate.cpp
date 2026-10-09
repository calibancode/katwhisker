// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "songdelegate.h"
#include "rowpainting.h"
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

void SongDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    RowPainting::paint(painter, option, index, QStringLiteral("edit-delete"), QStringLiteral("×"), true);
}
QSize SongDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return QSize(0, 56); }
bool SongDelegate::editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option,
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
            Q_EMIT removeRequested(index.data(Qt::UserRole).toMap());
            return true;
        }
    }
    return false;
}
