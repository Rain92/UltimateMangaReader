#include "mangainfo.h"

#include <QImage>

#include "configs.h"

MangaInfo::MangaInfo(QObject *parent, AbstractMangaSource *mangasource)
    : QObject(parent),
      updated(false),
      currentindex(0, 0),
      numchapters(0),
      mangasource(mangasource),
      updating(false),
      preloadqueue(parent, mangasource)
{
    QObject::connect(&preloadqueue, SIGNAL(completedDownload(QString)), this,
                     SLOT(completedImagePreload(QString)));
}

MangaInfo::~MangaInfo() = default;

QString MangaInfo::getCoverpathScaled() const
{
    if (coverpath == "")
        return "";

    QString scpath = coverpath;
    scpath.insert(scpath.length() - 4, "_scaled");

    QFileInfo scaledcover(scpath);
    if (!scaledcover.exists())
    {
        QImage img;
        img.load(coverpath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(scpath);
    }

    return scpath;
}

QString MangaInfo::getImageLink(MangaIndex index)
{
    if (index.chapter < 0 || index.chapter >= numchapters)
        return "";

    if (!chapters[index.chapter].pagesloaded)
    {
        chapters[index.chapter].loadPages();
        serialize();
    }

    if (index.chapter >= numchapters ||
        chapters[index.chapter].imagelinks.count() <= index.page)
        return "";

    if (chapters[index.chapter].imagelinks[index.page] == "")
        chapters[index.chapter].imagelinks[index.page] =
            mangasource->getImageLink(
                chapters[index.chapter].pagelinks.at(index.page));

    return chapters[index.chapter].imagelinks[index.page];
}

QString MangaInfo::getCurrentImage()
{
    QString imagelink = getImageLink(currentindex);

    return mangasource->downloadAwaitImage(
        imagelink, title, currentindex.chapter, currentindex.page);
}

QString MangaInfo::goChapterPage(MangaIndex index)
{
    QString imagelink = getImageLink(index);
    if (imagelink == "")
        return "";

    currentindex = index;

    return mangasource->downloadAwaitImage(
        imagelink, title, currentindex.chapter, currentindex.page);
}

QString MangaInfo::goNextPage()
{
    MangaIndex nextpage = currentindex.nextPageIndex(&chapters);
    if (nextpage.illegal)
        return "";

    currentindex = nextpage;

    return getCurrentImage();
}

QString MangaInfo::goPrevPage()
{
    MangaIndex prevpage = currentindex.prevPageIndex(&chapters);
    if (prevpage.illegal)
        return "";

    currentindex = prevpage;

    return getCurrentImage();
}

QString MangaInfo::goLastChapter()
{
    currentindex = MangaIndex(numchapters - 1, 0);

    return getCurrentImage();
}

QString MangaInfo::goFirstChapter()
{
    currentindex = MangaIndex(0, 0);

    return getCurrentImage();
}

void MangaInfo::preloadImage(MangaIndex index)
{
    if (!index.checkLegal(&chapters))
    {
        qDebug() << "illegal imagepath";
        return;
    }

    preloadqueue.addJob(DownloadImageInfo(getImageLink(index), title,
                                          index.chapter, index.page));
}

void MangaInfo::preloadPopular()
{
    if (numchapters == 0)
        return;
    preloadImage(currentindex);
    if (numchapters > 1 && currentindex.chapter != numchapters - 1)
        preloadImage(MangaIndex(numchapters - 1, 0));
}

void MangaInfo::preloadNeighbours(int forward, int backward)
{
    MangaIndex forwardindex = currentindex;
    MangaIndex backwardindex = currentindex;

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

void MangaInfo::cancelAllPreloads() { preloadqueue.clearQuene(); }

void MangaInfo::completedImagePreload(const QString &path)
{
    emit completedImagePreloadSignal(path);
}

QSharedPointer<MangaInfo> MangaInfo::deserialize(
    QObject *parent, AbstractMangaSource *mangasource, const QString &path)
{
    auto mi = QSharedPointer<MangaInfo>(new MangaInfo(parent, mangasource));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return mi;

    QDataStream in(&file);
    in >> mi->hostname >> mi->title >> mi->link >> mi->author >> mi->artist >>
        mi->releaseyear >> mi->status >> mi->genres >> mi->summary;
    in >> mi->coverlink >> mi->coverpath >> mi->numchapters >>
        mi->chapertitlesreversed >> mi->chapters;

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
    out << hostname << title << link << author << artist << releaseyear
        << status << genres << summary;
    out << coverlink << coverpath << (qint32)numchapters << chapertitlesreversed
        << chapters;

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
    out << currentindex << (qint32)numchapters;

    file.close();
}

void MangaInfo::deserializeProgress()
{
    QFile file(mangainfodir(hostname, title) + "progress.dat");

    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in2(&file);
    in2 >> this->currentindex;

    file.close();
}

void MangaInfo::sendUpdated(bool changed)
{
    updating = false;

    if (changed)
    {
        updated = true;
        emit updatedSignal();
    }
    else
        emit updatedNoChanges();
}

void MangaInfo::sendCoverLoaded() { emit coverLoaded(); }
