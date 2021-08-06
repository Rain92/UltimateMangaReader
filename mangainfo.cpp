#include "mangainfo.h"

MangaInfo::MangaInfo(AbstractMangaSource *mangasource)
    : QObject(), updated(false), mangaSource(mangasource), updateMutex(new QMutex())
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
    in >> mi->hostname >> mi->title >> mi->url >> mi->author >> mi->artist >> mi->releaseYear >> mi->status >>
        mi->genres >> mi->summary >> mi->coverUrl >> mi->coverPath >> mi->chapters;

    file.close();

    return mi;
}

void MangaInfo::serialize()
{
    QFile file(CONF.mangainfodir(hostname, title) + "mangainfo.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << hostname << title << url << author << artist << releaseYear << status << genres << summary
        << coverUrl << coverPath << chapters;

    file.close();
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

    emit updatedSignal(this->updated);
}

void MangaInfo::sendCoverLoaded()
{
    emit coverLoaded();
}

QString MangaInfo::coverThumbnailPath() const
{
    if (coverPath == "" || coverPath.length() < 4)
        return "";

    QString scpath = coverPath;
    scpath.insert(scpath.length() - 4, "_scaled");

    return scpath;
}
