#ifndef MANGACHAPTERCONTAINER_H
#define MANGACHAPTERCONTAINER_H

#include "mangachapter.h"

class MangaChapterCollection : public QList<MangaChapter>
{
public:
    MangaChapterCollection();

    QStringList getMangaTitlesReversed();

    void mergeChapters(MangaChapterCollection& newChapters);

    inline int numChapters() const { return size(); }
};

#endif  // MANGACHAPTERCONTAINER_H
