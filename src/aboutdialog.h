// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QPointer>
class AboutDialog : public QDialog
{
  public:
    explicit AboutDialog(QWidget *parent = nullptr);

  private:
    QPointer<QDialog> m_licenseDialog;
};
