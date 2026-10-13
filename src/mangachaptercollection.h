#pragma once

#include "mangachapter.h"

class MangaChapterCollection : public QList<MangaChapter>
{
public:
    MangaChapterCollection();

    struct MergeResult
    {
        // chapters that weren't there before (not for the first chapters of a manga)
        bool newChapters = false;
        // chapters that were there before are at another index now, or gone
        bool chaptersMoved = false;
        bool chaptersRemoved = false;
    };

    // Takes over the chapters of an update. The loaded pages of the chapters that are still there (with the
    // same url) are kept.
    MergeResult mergeChapters(const MangaChapterCollection &newChapters);

    // The index of the chapter with url, -1 if there is none.
    int findChapter(const QString &url) const;
};
