// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
class QLabel;
class QPushButton;
class QSlider;
class PlayerBar : public QWidget
{
    Q_OBJECT
  public:
    explicit PlayerBar(QWidget *parent = nullptr);
    QLabel *title, *detail, *artwork;
    QPushButton *favorite, *previous, *play, *next, *mute;
    QSlider *volume;
  Q_SIGNALS:
    void volumeScrolled(int steps);

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};
