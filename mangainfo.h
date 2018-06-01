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

    static MangaInfo *deserialize(QObject *parent, AbstractMangaSource *mangasource, const QString &path);

    void serialize();
    void serializeProgress();


    QString title;
    QString link;

    QString author;
    QString artist;
    QString releaseyear;
    QString genres;
    QString status;
    QString summary;
    QString coverpath;

    QString hostname;

    MangaIndex currentindex;

    int numchapters;

    AbstractMangaSource *mangasource;

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

    void preloadNeighbours(int distance = 3);
    void preloadChapter();
    void preloadImage(MangaIndex index);

    void preloadPopular();
    void cancelAllPreloads();


    void sendUpdated();

signals:
    void completedImagePreloadSignal(const QString &path);
    void updated();

private slots:
    void completedImagePreload(const QString &path);

private:
    PreloadQueue preloadqueue;

};

#endif // MANGAINFO_H
