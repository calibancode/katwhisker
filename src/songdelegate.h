// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPersistentModelIndex>
#include <QStyledItemDelegate>
class SongDelegate : public QStyledItemDelegate
{
    Q_OBJECT
  public:
    explicit SongDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
    bool editorEvent(QEvent *, QAbstractItemModel *, const QStyleOptionViewItem &, const QModelIndex &) override;
  Q_SIGNALS:
    void removeRequested(const QVariantMap &song);

  private:
    QPersistentModelIndex m_pressedAction;
};
