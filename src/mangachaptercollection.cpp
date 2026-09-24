#include "mangachaptercollection.h"

MangaChapterCollection::MangaChapterCollection() {}

QList<QPair<int, int>> MangaChapterCollection::mergeChapters(MangaChapterCollection& other)
{
    QList<QPair<int, int>> moveMapping;

    QVector<int> matches(this->length(), -1);
    QSet<int> claimed;

    for (int i = 0; i < this->length(); i++)
    {
        auto& ch = (*this)[i];
        if (!ch.pagesLoaded)
            continue;

        int i2 = i;
        if (!(i < other.length() && !claimed.contains(i) && ch.chapterTitle == other[i].chapterTitle &&
              ch.chapterUrl == other[i].chapterUrl))
        {
            for (i2 = 0; i2 < other.count(); i2++)
                if (!claimed.contains(i2) && ch.chapterTitle == other[i2].chapterTitle &&
                    ch.chapterUrl == other[i2].chapterUrl)
                    break;
        }

        if (i2 < other.length())
        {
            matches[i] = i2;
            claimed.insert(i2);
        }
    }

    // a source might have changed the title format or the domain of its chapters:
    // fall back to a unique match of either the url or the title to keep the loaded pages
    auto findUniqueUnclaimed = [&](const std::function<bool(const MangaChapter&)>& pred)
    {
        int found = -1;
        for (int j = 0; j < other.count(); j++)
        {
            if (!pred(other[j]))
                continue;
            if (found >= 0)
                return -1;
            found = j;
        }
        return claimed.contains(found) ? -1 : found;
    };

    // first all url matches, then the title matches
    for (bool byUrl : {true, false})
    {
        for (int i = 0; i < this->length(); i++)
        {
            auto& ch = (*this)[i];
            if (!ch.pagesLoaded || matches[i] >= 0)
                continue;

            int i2 = -1;
            if (byUrl && ch.chapterUrl != "")
                i2 = findUniqueUnclaimed([&ch](const MangaChapter& o) { return o.chapterUrl == ch.chapterUrl; });
            // a new url means stale page and image urls: only worth keeping when all pages are resolved
            else if (!byUrl && ch.chapterTitle.trimmed() != "" && !ch.imageUrlList.contains(""))
                i2 = findUniqueUnclaimed([&ch](const MangaChapter& o) { return o.chapterTitle == ch.chapterTitle; });

            if (i2 >= 0)
            {
                matches[i] = i2;
                claimed.insert(i2);
            }
        }
    }

    for (int i = 0; i < this->length(); i++)
    {
        auto& ch = (*this)[i];
        if (!ch.pagesLoaded)
            continue;

        int i2 = matches[i];
        if (i2 >= 0)
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
    this->clear();
    this->append(other);

    return moveMapping;
}
