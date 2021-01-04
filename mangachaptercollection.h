#ifndef MANGACHAPTERCONTAINER_H
#define MANGACHAPTERCONTAINER_H

#include "mangachapter.h"

class MangaChapterCollection : public QList<MangaChapter>
{
public:
    MangaChapterCollection();

    QStringList getMangaTitlesReversed();

    QList<QPair<int, int>> mergeChapters(MangaChapterCollection& newChapters);
};

#endif  // MANGACHAPTERCONTAINER_H
