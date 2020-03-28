#include "mangaindex.h"

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
    return str << (qint32)m.chapter << (qint32)m.page;
}

QDataStream &operator>>(QDataStream &str, MangaIndex &m)
{
    return str >> m.chapter >> m.page;
}
