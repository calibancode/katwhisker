// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later
#include "stationicons.h"
#include "useragent.h"
#include <QNetworkReply>
#include <QPixmap>

QIcon StationIcons::icon(const QString &url)
{
    const QUrl source(url);
    if (url.isEmpty() || (source.scheme() != QStringLiteral("https") && source.scheme() != QStringLiteral("http")))
        return QIcon::fromTheme(QStringLiteral("radio"));
    if (m_icons.contains(url))
        return m_icons.value(url);
    if (!m_pending.contains(url)) {
        m_pending.insert(url);
        QNetworkRequest request(source);
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
        request.setTransferTimeout(10000);
        auto *reply = m_network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
            QPixmap pixmap;
            if (reply->error() == QNetworkReply::NoError)
                pixmap.loadFromData(reply->readAll());
            const QIcon icon = pixmap.isNull()
                                   ? QIcon::fromTheme(QStringLiteral("radio"))
                                   : QIcon(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_icons.insert(url, icon);
            m_pending.remove(url);
            reply->deleteLater();
            Q_EMIT loaded(url);
        });
    }
    return QIcon::fromTheme(QStringLiteral("radio"));
}
