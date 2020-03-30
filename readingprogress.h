
#ifndef READINGPROGRESS_H
#define READINGPROGRESS_H

#include "mangaindex.h"
#include "staticsettings.h"

class ReadingProgress
{
public:
    MangaIndex index;
    int numPages;
    int numChapters;

    ReadingProgress(const MangaIndex& index, int numPages, int numChapters);
    ReadingProgress(const QString& hostname, const QString& title);

    void serialize(const QString& hostname, const QString& title);

    bool deserialize(const QString& hostname, const QString& title);
};

QDataStream& operator<<(QDataStream& str, const ReadingProgress& m);
QDataStream& operator>>(QDataStream& str, ReadingProgress& m);

#endif  // READINGPROGRESS_H
