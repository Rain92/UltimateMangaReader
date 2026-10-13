#include "readingprogress.h"

#include <QSaveFile>

#include "cachefile.h"

ReadingProgress::ReadingProgress() : index(0, 0), chapterUrl(), numChapters(0), numPages(0) {}

ReadingProgress::ReadingProgress(const MangaIndex &index, const QString &chapterUrl, int numChapters,
                                 int numPages)
    : index(index), chapterUrl(chapterUrl), numChapters(numChapters), numPages(numPages)
{
}

ReadingProgress::ReadingProgress(const QString &hostname, const QString &mangaUrl) : ReadingProgress()
{
    deserialize(hostname, mangaUrl);
}

void ReadingProgress::serialize(const QString &hostname, const QString &mangaUrl)
{
    QDir().mkpath(CONF.mangainfodir(hostname, mangaUrl));
    QSaveFile file(CONF.mangainfodir(hostname, mangaUrl) + "progress.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::ReadingProgress);
    out << *this;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the reading progress of" << mangaUrl;
}

bool ReadingProgress::deserialize(const QString &hostname, const QString &mangaUrl)
{
    QFile file(CONF.mangainfodir(hostname, mangaUrl) + "progress.dat");

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
