#include "mangachaptercollection.h"

MangaChapterCollection::MangaChapterCollection() {}

QStringList MangaChapterCollection::getMangaTitlesReversed()
{
    QStringList list;

    for (const auto& ch : qAsConst(*this))
        list.insert(0, ch.chapterTitle);

    return list;
}

QList<QPair<int, int>> MangaChapterCollection::mergeChapters(MangaChapterCollection& other)
{
    QList<int> changed;
    QList<QPair<int, int>> moveMapping;
    if (count() == other.count())
    {
        bool same = true;
        for (int i = 0; i < count(); i++)
        {
            if ((*this)[i].chapterUrl != other[i].chapterUrl ||
                (*this)[i].chapterTitle != other[i].chapterTitle)
            {
                same = false;
                changed.append(i);
            }
        }
        if (same)
            return moveMapping;
    }

    for (auto& i : changed)
    {
        auto& ch = (*this)[i];
        if (ch.pagesLoaded)
        {
            for (int i2 = 0; i2 < other.count(); i2++)
            {
                if (ch.chapterUrl == other[i2].chapterUrl && ch.chapterTitle == other[i2].chapterTitle)
                {
                    other[i2].imageUrlList = ch.imageUrlList;
                    other[i2].pageUrlList = ch.pageUrlList;
                    other[i2].pagesLoaded = true;
                    moveMapping.append({i, i2});
                    break;
                }
                else if (i2 == other.count() - 1)
                {
                    moveMapping.append({i, -1});
                }
            }
        }
    }
    (*this).clear();
    (*this).append(other);

    return moveMapping;
}
