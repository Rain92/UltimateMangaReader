#include "mangaindextraverser.h"

MangaIndexTraverser::MangaIndexTraverser(QSharedPointer<MangaInfo> mangainfo, int chapter, int page)
    : MangaIndex(chapter, page), mangaInfo(mangainfo)
{
}

Result<bool, QString> MangaIndexTraverser::increment()
{
    if (chapter < 0 || chapter >= mangaInfo->chapters.count())
        return Ok(false);

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
        return Ok(false);
    }
}

Result<bool, QString> MangaIndexTraverser::decrement()
{
    if (chapter < 0 || chapter >= mangaInfo->chapters.count())
        return Ok(false);

    if (page > 0)
    {
        return setChecked(chapter, page - 1);
    }
    else if (chapter > 0)
    {
        if (!mangaInfo->chapters.at(chapter - 1).pagesLoaded)
        {
            auto res = mangaInfo->mangaSource->updatePageList(mangaInfo, chapter - 1);
            if (!res.isOk())
                return Err(res.unwrapErr());
            // the next session would download it again
            mangaInfo->serialize();
        }

        return setChecked(chapter - 1, qMax(0, mangaInfo->chapters.at(chapter - 1).pages.count() - 1));
    }
    else
    {
        return Ok(false);
    }
}

Result<bool, QString> MangaIndexTraverser::setChecked(int chapter, int page)
{
    if (chapter < 0 || page < 0 || chapter >= mangaInfo->chapters.count() ||
        (chapter == this->chapter && page == this->page))
        return Ok(false);

    if (!mangaInfo->chapters.at(chapter).pagesLoaded)
    {
        auto res = mangaInfo->mangaSource->updatePageList(mangaInfo, chapter);
        if (!res.isOk())
            return Err(res.unwrapErr());
        mangaInfo->serialize();
    }

    if (page >= mangaInfo->chapters.at(chapter).pages.count())
        return Ok(false);

    this->chapter = chapter;
    this->page = page;

    return Ok(true);
}
