#include "mangainfo.h"

#include <QImage>

#include "defines.h"

MangaInfo::MangaInfo(AbstractMangaSource *mangasource)
    : QObject(),
      updated(false),
      currentIndex(0, 0),
      numChapters(0),
      mangaSource(mangasource),
      updating(false),
      preloadQueue(mangasource)
{
    QObject::connect(&preloadQueue, SIGNAL(completedDownload(QString)), this,
                     SLOT(completedImagePreload(QString)));
}

MangaInfo::~MangaInfo() = default;

QString MangaInfo::getCoverpathScaled() const
{
    if (coverPath == "" || coverPath.length() < 4)
        return "";

    QString scpath = coverPath;
    scpath.insert(scpath.length() - 4, "_scaled");

    if (!QFile::exists(scpath))
    {
        qDebug() << "generating scaled:" << title;
        QImage img;
        img.load(coverPath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(scpath);
    }

    return scpath;
}

QString MangaInfo::getImageLink(MangaIndex index)
{
    if (index.chapter < 0 || index.chapter >= numChapters)
        return "";

    if (!chapters[index.chapter].pagesLoaded)
    {
        chapters[index.chapter].loadPages();
        serialize();
    }

    if (index.chapter >= numChapters ||
        chapters[index.chapter].imagelinkList.count() <= index.page)
        return "";

    if (chapters[index.chapter].imagelinkList[index.page] == "")
        chapters[index.chapter].imagelinkList[index.page] =
            mangaSource->getImageLink(
                chapters[index.chapter].pagelinkList.at(index.page));

    return chapters[index.chapter].imagelinkList[index.page];
}

QString MangaInfo::getCurrentImage()
{
    QString imagelink = getImageLink(currentIndex);

    qDebug() << "getCurrentImage()";

    return mangaSource->downloadAwaitImage(DownloadImageDescriptor(
        imagelink, title, currentIndex.chapter, currentIndex.page));
}

QString MangaInfo::goChapterPage(MangaIndex index)
{
    QString imagelink = getImageLink(index);
    if (imagelink == "")
        return "";

    currentIndex = index;

    return mangaSource->downloadAwaitImage(DownloadImageDescriptor(
        imagelink, title, currentIndex.chapter, currentIndex.page));
}

QString MangaInfo::goNextPage()
{
    MangaIndex nextpage = currentIndex.nextPageIndex(&chapters);
    if (nextpage.illegal)
        return "";

    currentIndex = nextpage;

    return getCurrentImage();
}

QString MangaInfo::goPrevPage()
{
    MangaIndex prevpage = currentIndex.prevPageIndex(&chapters);
    if (prevpage.illegal)
        return "";

    currentIndex = prevpage;

    return getCurrentImage();
}

QString MangaInfo::goLastChapter()
{
    currentIndex = MangaIndex(numChapters - 1, 0);

    return getCurrentImage();
}

QString MangaInfo::goFirstChapter()
{
    currentIndex = MangaIndex(0, 0);

    return getCurrentImage();
}

void MangaInfo::preloadImage(MangaIndex index)
{
    if (!index.checkLegal(&chapters))
    {
        qDebug() << "illegal imagepath";
        return;
    }
    auto link = getImageLink(index);
    DownloadImageDescriptor imageinfo(link, title, index.chapter, index.page);

    if (QFile::exists(mangaSource->getImagePath(imageinfo)))
        return;

    //    qDebug() << "preload page" << index.page;

    preloadQueue.addJob(imageinfo);
}

void MangaInfo::preloadPopular()
{
    if (numChapters == 0)
        return;
    preloadImage(currentIndex);
    if (numChapters > 1 && currentIndex.chapter != numChapters - 1)
        preloadImage(MangaIndex(numChapters - 1, 0));
}

void MangaInfo::preloadNeighbours(int forward, int backward)
{
    MangaIndex forwardindex = currentIndex;
    MangaIndex backwardindex = currentIndex;

    for (int i = 0; i < qMax(forward, backward); i++)
    {
        if (i < forward)
        {
            forwardindex = forwardindex.nextPageIndex(&chapters);
            if (!forwardindex.illegal)
                preloadImage(forwardindex);
        }
        if (i < backward)
        {
            backwardindex = backwardindex.nextPageIndex(&chapters);
            if (!backwardindex.illegal)
                preloadImage(backwardindex);
        }
    }
}

void MangaInfo::cancelAllPreloads() { preloadQueue.clearQuene(); }

void MangaInfo::completedImagePreload(const QString &path)
{
    emit completedImagePreloadSignal(path);
}

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

    QMutableListIterator<MangaChapter> i(mi->chapters);
    while (i.hasNext())
    {
        MangaChapter &c = i.next();
        c.source = mangasource;
    }

    mi->deserializeProgress();

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

    if (!QFileInfo(mangainfodir(hostname, title) + "progress.dat").exists())
        serializeProgress();
}

void MangaInfo::serializeProgress()
{
    QFile file(mangainfodir(hostname, title) + "progress.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << currentIndex << (qint32)numChapters;

    file.close();
}

void MangaInfo::deserializeProgress()
{
    QFile file(mangainfodir(hostname, title) + "progress.dat");

    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in2(&file);
    in2 >> this->currentIndex;

    file.close();
}

void MangaInfo::updateCompeted(bool changed)
{
    updating = false;

    if (changed)
    {
        updated = true;
        emit updatedSignal();
    }
}

void MangaInfo::sendCoverLoaded() { emit coverLoaded(); }
