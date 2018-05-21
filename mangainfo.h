#ifndef MANGAINFO_H
#define MANGAINFO_H

#include "mangachapter.h"
#include "mangaindex.h"
#include "preloadqueue.h"



class MangaInfo : public QObject
{
    Q_OBJECT

public:
    MangaInfo(QObject *parent, AbstractMangaSource *mangasource);

    QString title;
    QString link;

    QString author;
    QString artist;
    QString releaseyear;
    QString status;
    QString summary;
    QString coverpath;

    QString hostname;

    MangaIndex currentindex;

    int numchapters;


    QStringList chapertitlesreversed;
    QList<MangaChapter> chapters;

    QString getImageLink(MangaIndex index);

    QString getCurrentImage();
    QString goNextPage();
    QString goPrevPage();
    QString goChapterPage(MangaIndex index);
    QString goNextChapter();
    QString goPrevChapter();
    QString goLastChapter();
    QString goFirstChapter();

    void PreloadNeighbours(int distance = 3);
    void PreloadChapter();
    void PreloadImage(MangaIndex index);

    void PreloadPopular();
    void CancelAllPreloads();

    AbstractMangaSource *mangasource;

signals:
    void completedImagePreloadSignal(QString path);

private slots:
    void completedImagePreload(QString path);

private:
    PreloadQueue preloadqueue;

};

#endif // MANGAINFO_H
