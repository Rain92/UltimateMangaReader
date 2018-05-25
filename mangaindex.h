#ifndef MANGAINDEX_H
#define MANGAINDEX_H

#include "mangachapter.h"

class  MangaIndex
{
public:
    int chapter;
    int page;
    bool illegal;

    MangaIndex(int chapter, int page);
    MangaIndex(int chapter, int page, bool illegal);



    MangaIndex nextPageIndex(QList<MangaChapter> *chapters);

    MangaIndex prevPageIndex(QList<MangaChapter> *chapters);

    MangaIndex deltaPageIndex(QList<MangaChapter> *chapters, int delta);

};


QDataStream &operator<<(QDataStream &str, const MangaIndex &m);
QDataStream &operator>>(QDataStream &str, MangaIndex &m);


#endif // MANGAINDEX_H
