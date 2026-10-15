#include "readingprogress.h"

#include <QSaveFile>

#include "cachefile.h"

ReadingProgress::ReadingProgress() : index(0, 0), chapterUrl(), numChapters(0), numPages(0) {}

ReadingProgress::ReadingProgress(const MangaIndex &index, const QString &chapterUrl, int numChapters,
                                 int numPages)
    : index(index), chapterUrl(chapterUrl), numChapters(numChapters), numPages(numPages)
{
}

ReadingProgress::ReadingProgress(const QString &sourceName, const QString &mangaUrl) : ReadingProgress()
{
    deserialize(sourceName, mangaUrl);
}

// the progress that serializeLater() kept, by the directory of the manga
static QHash<QString, ReadingProgress> &pending()
{
    static QHash<QString, ReadingProgress> progress;
    return progress;
}

static void write(const QString &dir, const ReadingProgress &progress)
{
    QDir().mkpath(dir);
    QSaveFile file(dir + "progress.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::ReadingProgress);
    out << progress;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the reading progress in" << dir;
}

void ReadingProgress::serialize(const QString &sourceName, const QString &mangaUrl)
{
    auto dir = CONF.mangainfodir(sourceName, mangaUrl);
    pending().remove(dir);
    write(dir, *this);
}

void ReadingProgress::serializeLater(const QString &sourceName, const QString &mangaUrl)
{
    pending().insert(CONF.mangainfodir(sourceName, mangaUrl), *this);
}

void ReadingProgress::flushPending()
{
    auto progress = std::exchange(pending(), {});
    for (auto it = progress.cbegin(); it != progress.cend(); ++it)
        write(it.key(), it.value());
}

bool ReadingProgress::deserialize(const QString &sourceName, const QString &mangaUrl)
{
    auto kept = pending().constFind(CONF.mangainfodir(sourceName, mangaUrl));
    if (kept != pending().cend())
    {
        *this = kept.value();
        return true;
    }

    QFile file(CONF.mangainfodir(sourceName, mangaUrl) + "progress.dat");

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    if (readCacheFileHeader(in, CacheFile::ReadingProgress))
        in >> *this;

    if (in.status() != QDataStream::Ok || index.chapter < 0 || index.page < 0)
    {
        qDebug() << "Couldn't read the reading progress of" << mangaUrl;
        *this = ReadingProgress();
        return false;
    }
    return true;
}

bool ReadingProgress::locate(const MangaChapterCollection &chapters)
{
    auto old = index;
    numChapters = chapters.count();

    int chapter = chapters.findChapter(chapterUrl);
    if (chapter >= 0)
    {
        index.chapter = chapter;
    }
    else if (!chapters.isEmpty())
    {
        index.chapter = qBound(0, index.chapter, static_cast<int>(chapters.count()) - 1);
        index.page = 0;
        chapterUrl = chapters[index.chapter].chapterUrl;
    }

    return index != old;
}

QDataStream &operator<<(QDataStream &str, const ReadingProgress &m)
{
    return str << m.index << m.chapterUrl << m.numChapters << m.numPages;
}

QDataStream &operator>>(QDataStream &str, ReadingProgress &m)
{
    return str >> m.index >> m.chapterUrl >> m.numChapters >> m.numPages;
}
