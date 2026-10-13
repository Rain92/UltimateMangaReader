#include "mangainfo.h"

#include <QSaveFile>

#include "perftimer.h"

MangaInfo::MangaInfo(AbstractMangaSource *mangasource)
    : QObject(), updated(false), mangaSource(mangasource)
{
}

MangaInfo::~MangaInfo() = default;

QSharedPointer<MangaInfo> MangaInfo::deserialize(AbstractMangaSource *mangasource, const QString &path)
{
    auto mi = QSharedPointer<MangaInfo>(new MangaInfo(mangasource));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return mi;

    QDataStream in(&file);
    in.setVersion(CONF.dataStreamVersion);
    in >> mi->hostname >> mi->title >> mi->url >> mi->author >> mi->artist >> mi->releaseYear >> mi->status >>
        mi->genres >> mi->summary >> mi->coverUrl >> mi->coverPath >> mi->chapters;

    file.close();

    // older versions stored absolute paths, the cover always lives in the manga's own directory
    if (QDir::isAbsolutePath(mi->coverPath))
        mi->coverPath = CONF.mangainfodir(mi->hostname, mi->title) + QFileInfo(mi->coverPath).fileName();
    else
        mi->coverPath = CONF.absoluteCachePath(mi->coverPath);

    return mi;
}

void MangaInfo::serialize()
{
    // an info that never loaded would end up in the root of the cache
    if (hostname.isEmpty() || title.isEmpty())
        return;

    PerfTimer perf("serialize info");
    QSaveFile file(CONF.mangainfodir(hostname, title) + "mangainfo.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out.setVersion(CONF.dataStreamVersion);
    out << hostname << title << url << author << artist << releaseYear << status << genres << summary
        << coverUrl << CONF.relativeCachePath(coverPath) << chapters;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the info of" << title;
}

void MangaInfo::updateCompeted(bool updated, const QList<QPair<int, int> > &moveMap)
{
    if (updated)
        this->updated = true;

    ReadingProgress progress(hostname, title);
    progress.numChapters = chapters.count();

    if (!moveMap.empty())
    {
        for (const auto &[i1, i2] : moveMap)
        {
            if (progress.index.chapter == i1)
            {
                if (i2 >= 0)
                    progress.index.chapter = i2;
                else
                {
                    progress.index.chapter = qMin(qMax(progress.index.chapter, 0), chapters.count() - 1);
                    progress.index.page = 0;
                }
                qDebug() << "Reading index changed for" << title << ":" << i1 << "->"
                         << progress.index.chapter;
                break;
            }
        }
        progress.serialize(hostname, title);
        emit chaptersMoved(moveMap);
    }

    emit updatedSignal(updated || !moveMap.empty());
}

void MangaInfo::sendCoverLoaded()
{
    emit coverLoaded();
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
