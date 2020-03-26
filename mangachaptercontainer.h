#ifndef MANGACHAPTERCONTAINER_H
#define MANGACHAPTERCONTAINER_H

#include "mangachapter.h"

class MangaChapterContainer : public QList<MangaChapter>
{
public:
    MangaChapterContainer();

    QStringList getMangaTitlesReversed();

    void mergeChapters(const MangaChapterContainer& newChapters);

    inline int numChapters() const { return size(); }
};

#endif  // MANGACHAPTERCONTAINER_H
