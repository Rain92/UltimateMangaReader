#ifndef MANGAINDEX_H
#define MANGAINDEX_H

#include "mangachaptercollection.h"

class MangaIndex
{
public:
    int chapter;
    int page;

    MangaIndex(int chapter, int page);
    MangaIndex(const MangaIndex &mangaIndex) = default;

    bool operator==(const MangaIndex &b) const;
    bool operator!=(const MangaIndex &b) const;
};

QDataStream &operator<<(QDataStream &str, const MangaIndex &m);
QDataStream &operator>>(QDataStream &str, MangaIndex &m);

#endif  // MANGAINDEX_H
