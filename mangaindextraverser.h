#ifndef MANGAINDEXTRAVERSER_H
#define MANGAINDEXTRAVERSER_H

#include "mangaindex.h"
#include "mangainfo.h"

class MangaIndexTraverser : public MangaIndex
{
private:
    QSharedPointer<MangaInfo> mangaInfo;

public:
    MangaIndexTraverser(QSharedPointer<MangaInfo> mangaInfo, int chapter,
                        int page);
    MangaIndexTraverser(const MangaIndexTraverser &mangaIndex) = default;

    bool increment();

    bool decrement();

    bool setChecked(int chapter, int page);

    inline MangaChapter &currentChapter()
    {
        return mangaInfo->chapters[chapter];
    };
    inline QString &currentPageUrl()
    {
        return currentChapter().pageUrlList[page];
    };
    inline QString &currentImageUrl()
    {
        return currentChapter().imageUrlList[page];
    };
};

#endif  // MANGAINDEXTRAVERSER_H
