#include "mangachaptercollection.h"

MangaChapterCollection::MangaChapterCollection() {}

MangaChapterCollection::MergeResult MangaChapterCollection::mergeChapters(const MangaChapterCollection &newChapters)
{
    MergeResult result;

    QHash<QString, int> newIndices;
    for (int i = 0; i < newChapters.count(); i++)
        newIndices.insert(newChapters[i].chapterUrl, i);

    MangaChapterCollection merged = newChapters;
    for (int i = 0; i < this->count(); i++)
    {
        const auto &chapter = (*this)[i];
        int i2 = newIndices.value(chapter.chapterUrl, -1);
        if (i2 < 0)
        {
            result.chaptersRemoved = true;
            result.chaptersMoved = true;
            continue;
        }
        if (i2 != i)
            result.chaptersMoved = true;

        if (chapter.pagesLoaded)
        {
            merged[i2].pagesLoaded = true;
            merged[i2].pages = chapter.pages;
        }
    }

    if (!this->isEmpty())
    {
        QSet<QString> oldUrls;
        for (const auto &chapter : std::as_const(*this))
            oldUrls.insert(chapter.chapterUrl);
        for (const auto &chapter : newChapters)
            if (!oldUrls.contains(chapter.chapterUrl))
                result.newChapters = true;
    }

    *this = merged;

    return result;
}

int MangaChapterCollection::findChapter(const QString &url) const
{
    for (int i = 0; i < this->count(); i++)
        if ((*this)[i].chapterUrl == url)
            return i;

    return -1;
}
