// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QListWidget>
#include <QTimer>

// A lightweight list with independently hoverable row actions and deferred
// end-of-list notifications, after insertions and scrollbar layout settle.
class ActionListWidget : public QListWidget
{
    Q_OBJECT
  public:
    explicit ActionListWidget(QWidget *parent = nullptr);
    ~ActionListWidget() override;
  Q_SIGNALS:
    void endReached();

  protected:
    bool viewportEvent(QEvent *event) override;

  private:
    QTimer *m_endCheck;
    QPoint m_hoverPosition{-1, -1};
};
