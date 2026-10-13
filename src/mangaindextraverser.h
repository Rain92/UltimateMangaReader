#pragma once

#include "mangaindex.h"
#include "mangainfo.h"
#include "mangarepository.h"

class MangaIndexTraverser : public MangaIndex
{
private:
    MangaRepository *repository;
    QSharedPointer<MangaInfo> mangaInfo;

public:
    // loads the page lists of the chapters it gets to (blocking, with an event loop)
    MangaIndexTraverser(MangaRepository *repository, QSharedPointer<MangaInfo> mangaInfo, int chapter,
                        int page);
    MangaIndexTraverser(const MangaIndexTraverser &mangaIndex) = default;

    Result<bool> increment();

    Result<bool> decrement();

    Result<bool> setChecked(int chapter, int page);

    inline MangaChapter &currentChapter()
    {
        return mangaInfo->chapters[chapter];
    };
};
