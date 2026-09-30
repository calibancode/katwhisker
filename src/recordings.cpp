// SPDX-FileCopyrightText: 2026 calibancode
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recordings.h"

#include <KLocalizedString>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThreadPool>
#include <QtEndian>

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

// MPEG-1 Layer III, the usual radio MP3. Returns the frame size in bytes for
// a header's 2nd and 3rd bytes, or 0 if it isn't one.
constexpr int SampleRates[] = {44100, 48000, 32000, 0};
int mp3FrameSize(quint8 b1, quint8 b2)
{
    static constexpr int bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    const int bitrate = bitrates[b2 >> 4];
    const int sampleRate = SampleRates[(b2 >> 2) & 3];
    if ((b1 & 0xfe) != 0xfa || bitrate == 0 || sampleRate == 0)
        return 0;
    return 144 * bitrate * 1000 / sampleRate + ((b2 >> 1) & 1);
}

// A Xing header frame, as encoders put at the start of an MP3: players read
// the exact duration and a seek table from it instead of guessing from the
// first frames' bitrate (which is wrong for VBR). With frames == 0 it's a
// placeholder of the same size. `offsets` are frame positions after it.
QByteArray xingFrame(const QByteArray &firstHeader, quint32 frames, const QList<qint64> &offsets, qint64 audioBytes)
{
    const int sampleRateIndex = (quint8(firstHeader[2]) >> 2) & 3;
    const bool mono = (quint8(firstHeader[3]) >> 6) == 3;
    // A 128 kbit/s frame fits the header at every sample rate.
    QByteArray frame(144 * 128 * 1000 / SampleRates[sampleRateIndex], '\0');
    frame[0] = char(0xff);
    frame[1] = char(0xfb); // MPEG-1 Layer III, no CRC
    frame[2] = char((9 << 4) | (sampleRateIndex << 2)); // 128 kbit/s, no padding
    frame[3] = firstHeader[3]; // same channel mode as the audio
    const qsizetype pos = 4 + (mono ? 17 : 32); // after the (empty) side info
    frame.replace(pos, 4, "Xing");
    if (frames == 0)
        return frame;
    const quint32 bytes = frame.size() + audioBytes;
    qToBigEndian<quint32>(0x7, frame.data() + pos + 4); // frames, bytes and seek table present
    qToBigEndian<quint32>(frames, frame.data() + pos + 8);
    qToBigEndian<quint32>(bytes, frame.data() + pos + 12);
    for (int i = 0; i < 100; ++i) {
        const qint64 offset = frame.size() + offsets.at(qint64(frames) * i / 100);
        frame[pos + 16 + i] = char(qMin<qint64>(255, offset * 256 / bytes));
    }
    return frame;
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
    // Let a running export finish before its source file is deleted.
    QThreadPool::globalInstance()->waitForDone();
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

void RecordingsModel::exportTo(int row, const QUrl &destination)
{
    if (row < 0 || row >= m_entries.size()) {
        Q_EMIT exported(i18n("No such recording"));
        return;
    }
    // Off the UI thread: a big file or a slow destination (USB stick,
    // network share) would otherwise freeze the window while copying.
    QThreadPool::globalInstance()->start([this, source = m_entries.at(row).path, target = destination.toLocalFile()] {
        QFile::remove(target); // the file dialog already confirmed overwriting
        QFile file(source);
        const auto error = file.copy(target) ? QString() : file.errorString();
        // Safe: the destructor waits for this task before `this` goes away.
        QMetaObject::invokeMethod(this, [this, error] { Q_EMIT exported(error); }, Qt::QueuedConnection);
    });
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
    m_mp3Header.clear();
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
    QByteArrayView chunk(data);
    if (m_awaitingFrame) {
        const auto sync = nextFrameSync(chunk);
        if (sync < 0)
            return;
        m_awaitingFrame = false;
        chunk = chunk.sliced(sync);
        // Standard MP3: reserve room for a Xing header, filled in by finish().
        if (m_extension == "mp3"_L1 && chunk.size() >= 4 && mp3FrameSize(chunk[1], chunk[2]) > 0) {
            m_mp3Header = chunk.first(4).toByteArray();
            m_file->write(xingFrame(m_mp3Header, 0, {}, 0));
        }
    }
    m_file->write(chunk.data(), chunk.size());
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

    qint64 exactDuration = duration;
    if (!m_mp3Header.isEmpty() && m_file->open(QIODevice::ReadWrite)) {
        // Count the frames after our placeholder, then fill it in place.
        const qsizetype start = xingFrame(m_mp3Header, 0, {}, 0).size();
        const QByteArray all = m_file->readAll();
        const QByteArray audio = all.size() >= start ? all.sliced(start) : QByteArray();
        QList<qint64> offsets;
        for (qsizetype i = 0; i + 3 <= audio.size();) {
            const int size = quint8(audio[i]) == 0xff ? mp3FrameSize(audio[i + 1], audio[i + 2]) : 0;
            if (size == 0 || i + size > audio.size())
                break; // not a frame, or the cut-short last one
            offsets.append(i);
            i += size;
        }
        if (offsets.size() >= 2) {
            m_file->seek(0);
            m_file->write(xingFrame(m_mp3Header, offsets.size(), offsets, audio.size()));
            exactDuration = qint64(offsets.size()) * 1152 * 1000 / SampleRates[(quint8(m_mp3Header[2]) >> 2) & 3];
        }
        m_file->close();
    }

    const auto title = m_title.isEmpty() ? m_station : m_title;
    const auto base = m_artist.isEmpty() ? title : m_artist + u" – "_s + title;
    m_model->add({title, m_artist, m_station, m_favicon, exactDuration, m_file->fileName(),
                  safeFileName(base) + u'.' + m_extension});
    m_file.reset();
}
