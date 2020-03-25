#include "mangainfo.h"

MangaInfo::MangaInfo(AbstractMangaSource *mangasource)
    : QObject(),
      updated(false),
      numChapters(0),
      mangaSource(mangasource),
      updating(false),
      updateMutex(new QMutex())
{
}

MangaInfo::~MangaInfo() = default;

QSharedPointer<MangaInfo> MangaInfo::deserialize(
    AbstractMangaSource *mangasource, const QString &path)
{
    auto mi = QSharedPointer<MangaInfo>(new MangaInfo(mangasource));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return mi;

    QDataStream in(&file);
    in >> mi->hostname >> mi->title >> mi->link >> mi->author >> mi->artist >>
        mi->releaseYear >> mi->status >> mi->genres >> mi->summary;
    in >> mi->coverLink >> mi->coverPath >> mi->numChapters >>
        mi->chaperTitleListDescending >> mi->chapters;

    file.close();

    return mi;
}

void MangaInfo::serialize()
{
    QFile file(mangainfodir(hostname, title) + "mangainfo.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << hostname << title << link << author << artist << releaseYear
        << status << genres << summary;
    out << coverLink << coverPath << (qint32)numChapters
        << chaperTitleListDescending << chapters;

    file.close();
}

void MangaInfo::updateCompeted(bool newchapters)
{
    updating = false;

    if (newchapters)
        updated = true;

    emit updatedSignal(newchapters);
}

void MangaInfo::sendCoverLoaded() { emit coverLoaded(); }

QString MangaInfo::coverThumbnailPath() const
{
    if (coverPath == "" || coverPath.length() < 4)
        return "";

    QString scpath = coverPath;
    scpath.insert(scpath.length() - 4, "_scaled");

    return scpath;
}
