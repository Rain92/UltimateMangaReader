#include "mangainfo.h"

MangaInfo::MangaInfo(AbstractMangaSource *mangasource)
    : QObject(),
      updated(false),
      mangaSource(mangasource),
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
        mi->releaseYear >> mi->status >> mi->genres >> mi->summary >>
        mi->coverLink >> mi->coverPath >> mi->chapters;

    file.close();

    return mi;
}

void MangaInfo::serialize()
{
    QFile file(CONF.mangainfodir(hostname, title) + "mangainfo.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << hostname << title << link << author << artist << releaseYear
        << status << genres << summary << coverLink << coverPath << chapters;

    file.close();
}

void MangaInfo::updateCompeted(bool newchapters)
{
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
