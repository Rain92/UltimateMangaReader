#pragma once

#include "mangaindex.h"
#include "staticsettings.h"

class ReadingProgress
{
public:
    MangaIndex index;
    // the chapter of the index: the chapters can change (see locate())
    QString chapterUrl;
    int numChapters;
    int numPages;

    ReadingProgress();
    ReadingProgress(const MangaIndex& index, const QString& chapterUrl, int numChapters, int numPages);
    // The progress of the manga, the start if there is none.
    ReadingProgress(const QString& sourceName, const QString& mangaUrl);

    void serialize(const QString& sourceName, const QString& mangaUrl);

    bool deserialize(const QString& sourceName, const QString& mangaUrl);

    // Finds the chapter by its url after the chapters changed. When it's gone (or there was no progress yet)
    // the start of the chapter at its index is taken. Returns whether the index changed.
    bool locate(const MangaChapterCollection& chapters);
};

QDataStream& operator<<(QDataStream& str, const ReadingProgress& m);
QDataStream& operator>>(QDataStream& str, ReadingProgress& m);
