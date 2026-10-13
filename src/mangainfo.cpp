#include "mangainfo.h"

#include <QSaveFile>

#include "cachefile.h"
#include "perftimer.h"

MangaInfo::MangaInfo(MangaSourceBase *mangasource)
    : QObject(), updated(false), mangaSource(mangasource)
{
}

MangaInfo::~MangaInfo() = default;

QSharedPointer<MangaInfo> MangaInfo::deserialize(MangaSourceBase *mangasource, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    auto mi = QSharedPointer<MangaInfo>(new MangaInfo(mangasource));

    QDataStream in(&file);
    if (readCacheFileHeader(in, CacheFile::MangaInfo))
        in >> mi->hostname >> mi->title >> mi->url >> mi->author >> mi->artist >> mi->releaseYear >> mi->status >>
            mi->genres >> mi->summary >> mi->coverUrl >> mi->coverPath >> mi->chapters;

    if (in.status() != QDataStream::Ok || mi->url.isEmpty())
    {
        qDebug() << "Couldn't read" << path;
        return {};
    }

    mi->coverPath = CONF.absoluteCachePath(mi->coverPath);

    for (auto &chapter : mi->chapters)
        for (auto &page : chapter.pages)
            page.image = {};

    return mi;
}

void MangaInfo::serialize()
{
    // an info that never loaded would end up in the root of the cache
    if (hostname.isEmpty() || url.isEmpty())
        return;

    PerfTimer perf("serialize info");
    QDir().mkpath(directory());
    QSaveFile file(directory() + "mangainfo.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::MangaInfo);
    out << hostname << title << url << author << artist << releaseYear << status << genres << summary
        << coverUrl << CONF.relativeCachePath(coverPath) << chapters;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the info of" << title;
}

void MangaInfo::updateCompleted(const MangaChapterCollection::MergeResult &merge)
{
    if (merge.newChapters)
        updated = true;

    if (merge.chaptersMoved)
    {
        // the favorites show the index of the chapter
        ReadingProgress progress;
        if (progress.deserialize(hostname, url) && progress.locate(chapters))
        {
            qDebug() << "Reading index changed for" << title << "to" << progress.index.chapter;
            progress.serialize(hostname, url);
        }
        emit chaptersMoved();
    }

    emit updatedSignal(merge.newChapters);
}

void MangaInfo::sendCoverLoaded()
{
    emit coverLoaded();
}

QString MangaInfo::directory() const
{
    return CONF.mangainfodir(hostname, url);
}

QString MangaInfo::pagePath(const QString &chapterUrl, int page) const
{
    return CONF.mangaimagesdir(hostname, url) + CONF.pagefilename(chapterUrl, page);
}

QString MangaInfo::pagePath(int chapter, int page) const
{
    if (chapter < 0 || chapter >= chapters.count())
        return {};

    return pagePath(chapters[chapter].chapterUrl, page);
}

QString MangaInfo::coverThumbnailPath() const
{
    if (coverPath == "" || coverPath.length() < 4)
        return "";

    // thumbnails are saved as jpg unless the cover is a png
    QFileInfo coverInfo(coverPath);
    QString filetype = coverInfo.suffix().toLower() == "png" ? ".png" : ".jpg";

    return coverInfo.path() + "/" + coverInfo.completeBaseName() + "_scaled" + filetype;
}
