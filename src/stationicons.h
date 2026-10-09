// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QSet>
class StationIcons : public QObject
{
    Q_OBJECT
  public:
    explicit StationIcons(QObject *parent = nullptr) : QObject(parent) {}
    QIcon icon(const QString &url);
  Q_SIGNALS:
    void loaded(const QString &url);

  private:
    QNetworkAccessManager m_network;
    QHash<QString, QIcon> m_icons;
    QSet<QString> m_pending;
};
