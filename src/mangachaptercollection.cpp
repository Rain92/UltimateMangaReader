#include "mangachaptercollection.h"

MangaChapterCollection::MangaChapterCollection() {}

QList<QPair<int, int>> MangaChapterCollection::mergeChapters(MangaChapterCollection& other)
{
    QList<QPair<int, int>> moveMapping;

    for (int i = 0; i < this->length(); i++)
    {
        auto& ch = (*this)[i];
        if (ch.pagesLoaded)
        {
            int i2 = i;
            if (!(i < other.length() && ch.chapterTitle == other[i].chapterTitle &&
                  ch.chapterUrl == other[i].chapterUrl))
            {
                for (i2 = 0; i2 < other.count(); i2++)
                    if (ch.chapterTitle == other[i2].chapterTitle && ch.chapterUrl == other[i2].chapterUrl)
                        break;
            }

            if (i2 < other.length())
            {
                other[i2].imageUrlList = ch.imageUrlList;
                other[i2].pageUrlList = ch.pageUrlList;
                other[i2].pagesLoaded = true;
                if (i != i2)
                    moveMapping.append({i, i2});
            }
            else
            {
                moveMapping.append({i, -1});
            }
        }
    }
    this->clear();
    this->append(other);

    return moveMapping;
}
