// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QQmlEngine>
#include <QUrl>

#include <memory>

// Songs recorded from the stream, kept in a temporary directory until the app
// restarts. Each entry is the station's original bytes for one song, so
// recordings are lossless copies of what was broadcast.
class RecordingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by StreamProxy")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString recordingTitle READ recordingTitle NOTIFY recordingTitleChanged)

public:
    enum Role { TitleRole = Qt::UserRole + 1, ArtistRole, StationRole, FaviconRole, DurationRole, FileNameRole };

    struct Entry {
        QString title;
        QString artist;
        QString station;
        QString favicon;
        qint64 durationMs = 0;
        QString path;
        QString fileName; // suggested export name
    };

    explicit RecordingsModel(QObject *parent = nullptr);
    ~RecordingsModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString recordingTitle() const { return m_recordingTitle; }

    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void clear();
    // Copies the recording to `destination`; returns an error message or "".
    Q_INVOKABLE QString exportTo(int row, const QUrl &destination) const;

    // Used by the recorder.
    QString newTempPath(const QString &extension);
    void add(Entry entry);
    void setRecordingTitle(const QString &title);

Q_SIGNALS:
    void countChanged();
    void recordingTitleChanged();

private:
    QDir m_dir;
    QList<Entry> m_entries; // newest first
    QString m_recordingTitle;
    int m_counter = 0;
};

// Cuts one connection's stream into songs at the boundaries the proxy
// reports. The first song is skipped, since we joined it midway.
class TrackRecorder
{
public:
    // `frameAligned`: cuts may land mid-frame (ICY streams), so each song
    // should start at the next MP3/AAC frame header.
    TrackRecorder(RecordingsModel *model, QString station, QString favicon, QString extension, bool frameAligned);
    ~TrackRecorder();

    // A new song starts at the next write(). Title/artist may come later.
    void boundary();
    void setSong(const QString &artist, const QString &title);
    void write(const QByteArray &data);

private:
    void finish();

    RecordingsModel *m_model;
    QString m_station;
    QString m_favicon;
    QString m_extension;
    bool m_frameAligned;
    int m_boundaries = 0;
    std::unique_ptr<QFile> m_file;
    QElapsedTimer m_elapsed;
    QString m_artist;
    QString m_title;
    bool m_tooLong = false;
    bool m_awaitingFrame = false;
};
