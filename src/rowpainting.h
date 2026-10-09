// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QIcon>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QWidget>

namespace RowPainting
{
inline QRect actionRect(const QRect &row) { return QRect(row.right() - 40, row.center().y() - 16, 32, 32); }
inline void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
                  const QString &actionIcon, const QString &fallback, bool song)
{
    painter->save();
    const auto row = option.rect.adjusted(4, 2, -4, -2);
    const bool active = !song && index.data(Qt::UserRole + 2).toBool();
    const bool selected = active || (option.state & QStyle::State_Selected);
    const bool hovered = option.state & QStyle::State_MouseOver;
    // The theme's selection color is already toned for its background. Keep
    // most of that tint, including when another window has focus.
    QColor background = option.palette.color(QPalette::Active, QPalette::Highlight);
    background.setAlphaF(selected ? (hovered ? 0.70 : 0.60) : hovered ? 0.25 : 0.0);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(background);
    painter->drawRoundedRect(row, 4, 4);
    const QRect iconRect(row.left() + 6, row.center().y() - 16, 32, 32);
    qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).paint(painter, iconRect);
    const auto lines = index.data(Qt::DisplayRole).toString().split(u'\n');
    const bool tags = !song && index.data(Qt::UserRole + 3).toBool() && lines.size() > 2 && !lines[2].isEmpty();
    QFont titleFont = option.font;
    titleFont.setBold(active);
    QFont smallFont = option.font;
    smallFont.setBold(false);
    smallFont.setPointSizeF(qMax(8.0, smallFont.pointSizeF() - 1));
    const QFontMetrics titleMetrics(titleFont), smallMetrics(smallFont);
    const int total = titleMetrics.height() + smallMetrics.height() + (tags ? smallMetrics.height() + 2 : 0);
    const int left = iconRect.right() + 9;
    const int width = qMax(0, actionRect(option.rect).left() - left - 8);
    int top = row.center().y() - total / 2;
    painter->setFont(titleFont);
    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(QRect(left, top, width, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(lines.value(0), Qt::ElideRight, width));
    top += titleMetrics.height();
    painter->setFont(smallFont);
    QColor secondary = option.palette.color(QPalette::Text);
    secondary.setAlpha(170);
    painter->setPen(secondary);
    painter->drawText(QRect(left, top, width, smallMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      smallMetrics.elidedText(lines.value(1), Qt::ElideRight, width));
    if (tags) {
        painter->setPen(option.palette.color(QPalette::Link));
        painter->drawText(QRect(left, top + smallMetrics.height() + 2, width, smallMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, smallMetrics.elidedText(lines[2], Qt::ElideRight, width));
    }
    const auto hover = option.widget ? option.widget->property("actionHoverPosition") : QVariant();
    const bool actionHovered = hover.isValid() && actionRect(option.rect).contains(hover.toPoint());
    if (actionHovered) {
        QColor fill = option.palette.color(QPalette::Text);
        const bool pressed = option.widget->property("actionPressed").toBool();
        fill.setAlpha(pressed ? 38 : 20);
        QColor border = option.palette.color(QPalette::Text);
        border.setAlpha(pressed ? 80 : 45);
        painter->setPen(QPen(border, 1));
        painter->setBrush(fill);
        painter->drawRoundedRect(actionRect(option.rect).adjusted(0, 0, -1, -1), 4, 4);
    }
    const auto action = QIcon::fromTheme(actionIcon);
    if (action.isNull()) {
        QFont font = option.font;
        font.setPointSizeF(font.pointSizeF() + 5);
        painter->setFont(font);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(actionRect(option.rect), Qt::AlignCenter, fallback);
    } else
        action.paint(painter, actionRect(option.rect).adjusted(6, 6, -6, -6));
    if (option.state & QStyle::State_HasFocus) {
        QColor focus = option.palette.color(QPalette::Active, QPalette::Highlight);
        focus.setAlpha(160);
        painter->setPen(QPen(focus, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(row.adjusted(0, 0, -1, -1), 4, 4);
    }
    painter->restore();
}
} // namespace RowPainting
