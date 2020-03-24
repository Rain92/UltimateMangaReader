#ifndef MANGAINDEX_H
#define MANGAINDEX_H

#include "mangachapter.h"
#include "mangachaptercollection.h"
#include "mangainfo.h"

class MangaIndex
{
public:
    int chapter;
    int page;

    MangaIndex(QSharedPointer<MangaInfo> mangainfo, int chapter, int page);
    MangaIndex(const MangaIndex &mangaIndex) = default;

    bool increment();

    bool decrement();

    bool setChecked(int chapter, int page);

    bool operator==(const MangaIndex &b) const;
    bool operator!=(const MangaIndex &b) const;

private:
    QSharedPointer<MangaInfo> mangainfo;
};

QDataStream &operator<<(QDataStream &str, const MangaIndex &m);
QDataStream &operator>>(QDataStream &str, MangaIndex &m);

#endif  // MANGAINDEX_H
