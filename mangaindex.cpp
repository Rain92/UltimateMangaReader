#include "mangaindex.h"

MangaIndex::MangaIndex(QSharedPointer<MangaInfo> mangainfo, int chapter,
                       int page)
    : chapter(chapter), page(page), mangainfo(mangainfo)
{
}

bool MangaIndex::increment()
{
    if (page + 1 < mangainfo->chapters.at(chapter).numPages)
    {
        page++;
        return true;
    }
    else if (chapter + 1 < mangainfo->chapters.count())
    {
        if (!mangainfo->chapters.at(chapter + 1).pagesLoaded)
            mangainfo->mangaSource->updatePageList(mangainfo, chapter);

        page = 0;
        chapter++;
        return true;
    }
    else
    {
        return false;
    }
}

bool MangaIndex::decrement()
{
    if (page > 0)
    {
        page--;
        return true;
    }
    else if (chapter > 0)
    {
        if (!mangainfo->chapters.at(chapter - 1).pagesLoaded)
            mangainfo->mangaSource->updatePageList(mangainfo, chapter);

        chapter--;
        page = mangainfo->chapters.at(chapter).numPages - 1;

        return true;
    }
    else
    {
        return false;
    }
}

bool MangaIndex::setChecked(int chapter, int page)
{
    if (chapter < 0 || page < 0 || chapter >= mangainfo->chapters.count() ||
        (chapter == this->chapter && page == this->page))
        return false;

    if (!mangainfo->chapters.at(chapter).pagesLoaded)
        mangainfo->mangaSource->updatePageList(mangainfo, chapter);

    if (page >= mangainfo->chapters.at(chapter).numPages)
        return false;

    this->chapter = chapter;
    this->page = page;

    return true;
}

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
