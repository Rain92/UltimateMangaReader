#include "mangaindex.h"

MangaIndex::MangaIndex() : chapter(0), page(0) {}
MangaIndex::MangaIndex(int chapter, int page) : chapter(chapter), page(page) {}

bool MangaIndex::operator==(const MangaIndex &b) const
{
    return page == b.page && chapter == b.chapter;
}

bool MangaIndex::operator!=(const MangaIndex &b) const
{
    return page == b.page || chapter != b.chapter;
}

QDataStream &operator<<(QDataStream &str, const MangaIndex &m)
{
    return str << m.chapter << m.page;
}

QDataStream &operator>>(QDataStream &str, MangaIndex &m)
{
    return str >> m.chapter >> m.page;
}
