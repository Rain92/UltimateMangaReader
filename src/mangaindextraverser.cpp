#include "mangaindextraverser.h"

MangaIndexTraverser::MangaIndexTraverser(MangaRepository *repository, QSharedPointer<MangaInfo> mangainfo,
                                         int chapter, int page)
    : MangaIndex(chapter, page), repository(repository), mangaInfo(mangainfo)
{
}

Result<bool> MangaIndexTraverser::increment()
{
    if (chapter < 0 || chapter >= mangaInfo->chapters.count())
        return false;

    if (page + 1 < mangaInfo->chapters.at(chapter).pages.count())
    {
        return setChecked(chapter, page + 1);
    }
    else if (chapter + 1 < mangaInfo->chapters.count())
    {
        return setChecked(chapter + 1, 0);
    }
    else
    {
        return false;
    }
}

Result<bool> MangaIndexTraverser::decrement()
{
    if (chapter < 0 || chapter >= mangaInfo->chapters.count())
        return false;

    if (page > 0)
    {
        return setChecked(chapter, page - 1);
    }
    else if (chapter > 0)
    {
        if (!mangaInfo->chapters.at(chapter - 1).pagesLoaded)
        {
            auto res = repository->loadPageList(mangaInfo, chapter - 1).wait();
            if (!res.has_value())
                return Err(res.error());
            // the next session would download it again
            mangaInfo->serialize();
        }

        return setChecked(chapter - 1, qMax(0, mangaInfo->chapters.at(chapter - 1).pages.count() - 1));
    }
    else
    {
        return false;
    }
}

Result<bool> MangaIndexTraverser::setChecked(int chapter, int page)
{
    if (chapter < 0 || page < 0 || chapter >= mangaInfo->chapters.count() ||
        (chapter == this->chapter && page == this->page))
        return false;

    if (!mangaInfo->chapters.at(chapter).pagesLoaded)
    {
        auto res = repository->loadPageList(mangaInfo, chapter).wait();

        if (!res.has_value())
            return Err(res.error());
        mangaInfo->serialize();
    }

    if (page >= mangaInfo->chapters.at(chapter).pages.count())
        return false;

    this->chapter = chapter;
    this->page = page;

    return true;
}
