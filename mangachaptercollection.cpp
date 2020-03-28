#include "mangachaptercollection.h"

MangaChapterCollection::MangaChapterCollection() {}

QStringList MangaChapterCollection::getMangaTitlesReversed()
{
    QStringList list;

    for (const auto& ch : *this)
        list.insert(0, ch.chapterTitle);

    return list;
}

void MangaChapterCollection::mergeChapters(MangaChapterCollection& other)
{
    if (numChapters() == other.numChapters())
    {
        bool same = true;
        for (int i = 0; i < numChapters(); i++)
        {
            if ((*this)[i].chapterUrl != other[i].chapterUrl ||
                (*this)[i].chapterTitle != other[i].chapterTitle)
            {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }

    for (auto& ch : *this)
    {
        if (ch.pagesLoaded)
        {
            for (int i = 0; i < other.numChapters(); i++)
            {
                if (ch.chapterUrl == other[i].chapterUrl ||
                    ch.chapterTitle == other[i].chapterTitle)
                {
                    other[i].imageUrlList = ch.imageUrlList;
                    other[i].pageUrlList = ch.pageUrlList;
                    other[i].pagesLoaded = true;
                    break;
                }
            }
        }
    }
    (*this).clear();
    (*this).append(other);
}
