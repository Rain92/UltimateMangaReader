#include "mangachaptercontainer.h"

MangaChapterContainer::MangaChapterContainer() {}

QStringList MangaChapterContainer::getMangaTitlesReversed()
{
    QStringList list;

    for (auto& ch : *this)
        list.insert(0, ch.chapterTitle);

    return list;
}

void MangaChapterContainer::mergeChapters(const MangaChapterContainer& other)
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
        }
    }
}
