// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recordings.h"

#include <KLocalizedString>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr qint64 MinimumMs = 30 * 1000; // shorter is a jingle or station ID
constexpr qint64 MaximumMs = 15 * 60 * 1000; // longer is a show, not a song
constexpr int MaximumCount = 20;

// Offset of the next MP3 or AAC (ADTS) frame header, or -1.
qsizetype nextFrameSync(QByteArrayView data)
{
    for (qsizetype i = 0; i + 3 < data.size(); ++i) {
        const auto b0 = quint8(data[i]), b1 = quint8(data[i + 1]), b2 = quint8(data[i + 2]);
        if (b0 != 0xff)
            continue;
        const bool adts = (b1 & 0xf6) == 0xf0;
        const bool mpeg = (b1 & 0xe0) == 0xe0 && ((b1 >> 1) & 3) != 0 // layer
            && (b2 >> 4) != 0 && (b2 >> 4) != 0xf // bitrate
            && ((b2 >> 2) & 3) != 3; // sample rate
        if (adts || mpeg)
            return i;
    }
    return -1;
}

QString safeFileName(QString name)
{
    static const QRegularExpression forbidden(u"[/\\\\:*?\"<>|\\x00-\\x1f]"_s);
    name.replace(forbidden, u"_"_s);
    return name.trimmed().left(180);
}
}

RecordingsModel::RecordingsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/recording"_s)
{
    // Wiped at startup rather than exit, so a crash doesn't leave files behind.
    m_dir.removeRecursively();
    m_dir.mkpath(u"."_s);
}

RecordingsModel::~RecordingsModel()
{
    m_dir.removeRecursively();
}

int RecordingsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant RecordingsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const auto &e = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
        return e.title;
    case ArtistRole:
        return e.artist;
    case StationRole:
        return e.station;
    case FaviconRole:
        return e.favicon;
    case DurationRole:
        return e.durationMs;
    case FileNameRole:
        return e.fileName;
    }
    return {};
}

QHash<int, QByteArray> RecordingsModel::roleNames() const
{
    return {{TitleRole, "title"}, {ArtistRole, "artist"}, {StationRole, "station"}, {FaviconRole, "favicon"},
            {DurationRole, "duration"}, {FileNameRole, "fileName"}};
}

void RecordingsModel::remove(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    beginRemoveRows({}, row, row);
    QFile::remove(m_entries.takeAt(row).path);
    endRemoveRows();
    Q_EMIT countChanged();
}

void RecordingsModel::clear()
{
    beginResetModel();
    for (const auto &e : std::as_const(m_entries))
        QFile::remove(e.path);
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
}

QString RecordingsModel::exportTo(int row, const QUrl &destination) const
{
    if (row < 0 || row >= m_entries.size())
        return i18n("No such recording");
    const auto target = destination.toLocalFile();
    QFile::remove(target); // the file dialog already confirmed overwriting
    QFile source(m_entries.at(row).path);
    if (!source.copy(target))
        return source.errorString();
    return {};
}

QString RecordingsModel::newTempPath(const QString &extension)
{
    return m_dir.filePath(u"%1.%2"_s.arg(++m_counter).arg(extension));
}

void RecordingsModel::add(Entry entry)
{
    beginInsertRows({}, 0, 0);
    m_entries.prepend(std::move(entry));
    endInsertRows();
    while (m_entries.size() > MaximumCount)
        remove(m_entries.size() - 1);
    Q_EMIT countChanged();
}

void RecordingsModel::setRecordingTitle(const QString &title)
{
    if (m_recordingTitle != title) {
        m_recordingTitle = title;
        Q_EMIT recordingTitleChanged();
    }
}

TrackRecorder::TrackRecorder(RecordingsModel *model, QString station, QString favicon, QString extension, bool frameAligned)
    : m_model(model)
    , m_station(std::move(station))
    , m_favicon(std::move(favicon))
    , m_extension(std::move(extension))
    , m_frameAligned(frameAligned)
{
}

TrackRecorder::~TrackRecorder()
{
    // Stopped mid-song: that one's incomplete.
    if (m_file) {
        m_file->remove();
        m_model->setRecordingTitle({});
    }
}

void TrackRecorder::boundary()
{
    finish();
    // The first boundary is where we joined; songs start at the ones after.
    if (++m_boundaries < 2)
        return;
    m_file = std::make_unique<QFile>(m_model->newTempPath(m_extension));
    if (!m_file->open(QIODevice::WriteOnly)) {
        m_file.reset();
        return;
    }
    m_elapsed.start();
    m_artist.clear();
    m_title.clear();
    m_tooLong = false;
    m_awaitingFrame = m_frameAligned;
}

void TrackRecorder::setSong(const QString &artist, const QString &title)
{
    if (!m_file)
        return;
    m_artist = artist;
    m_title = title;
    m_model->setRecordingTitle(title.isEmpty() ? m_station : title);
}

void TrackRecorder::write(const QByteArray &data)
{
    if (!m_file || m_tooLong)
        return;
    if (m_elapsed.elapsed() > MaximumMs) {
        m_tooLong = true;
        return;
    }
    if (m_awaitingFrame) {
        const auto sync = nextFrameSync(data);
        if (sync < 0)
            return;
        m_awaitingFrame = false;
        m_file->write(data.sliced(sync));
        return;
    }
    m_file->write(data);
}

void TrackRecorder::finish()
{
    if (!m_file)
        return;
    const qint64 duration = m_elapsed.elapsed();
    m_file->close();
    m_model->setRecordingTitle({});

    if (m_tooLong || duration < MinimumMs || m_file->size() == 0) {
        m_file->remove();
        m_file.reset();
        return;
    }

    const auto title = m_title.isEmpty() ? m_station : m_title;
    const auto base = m_artist.isEmpty() ? title : m_artist + u" – "_s + title;
    m_model->add({title, m_artist, m_station, m_favicon, duration, m_file->fileName(),
                  safeFileName(base) + u'.' + m_extension});
    m_file.reset();
}
