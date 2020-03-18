#include "mangaindex.h"

MangaIndex::MangaIndex(int chapter, int page)
    : chapter(chapter), page(page), illegal(false)
{
}

MangaIndex::MangaIndex(int chapter, int page, bool illegal)
    : chapter(chapter), page(page), illegal(illegal)
{
}

MangaIndex MangaIndex::nextPageIndex(QList<MangaChapter> *chapters)
{
    if (illegal)
        return MangaIndex(-1, -1, true);

    if (page < chapters->at(chapter).numpages - 1)
    {
        return MangaIndex(chapter, page + 1);
    }
    else if (chapter < chapters->count() - 1)
    {
        return MangaIndex(chapter + 1, 0);
    }
    else
    {
        return MangaIndex(-1, -1, true);
    }
}
MangaIndex MangaIndex::prevPageIndex(QList<MangaChapter> *chapters)
{
    if (illegal)
        return MangaIndex(-1, -1, true);

    if (page > 0)
    {
        return MangaIndex(chapter, page - 1);
    }
    else if (chapter > 0)
    {
        if (!chapters->at(chapter - 1).pagesloaded)
            (*chapters)[chapter - 1].loadPages();

        return MangaIndex(chapter - 1, chapters->at(chapter - 1).numpages - 1);
    }
    else
    {
        return MangaIndex(-1, -1, true);
    }
}

MangaIndex MangaIndex::deltaPageIndex(QList<MangaChapter> *chapters, int delta)
{
    if (delta == 0)
    {
        return MangaIndex(chapter, page);
    }
    else if (delta > 0)
    {
        return nextPageIndex(chapters).deltaPageIndex(chapters, delta - 1);
    }
    else
    {
        return prevPageIndex(chapters).deltaPageIndex(chapters, delta + 1);
    }
}

bool MangaIndex::checkLegal(QList<MangaChapter> *chapters)
{
    return chapter >= 0 && page >= 0 && chapters->length() > chapter &&
           (!chapters->at(chapter).pagesloaded ||
            chapters->at(chapter).numpages > page);
}

QDataStream &operator<<(QDataStream &str, const MangaIndex &m)
{
    return str << (qint32)m.chapter << (qint32)m.page;
}

QDataStream &operator>>(QDataStream &str, MangaIndex &m)
{
    return str >> m.chapter >> m.page;
}
