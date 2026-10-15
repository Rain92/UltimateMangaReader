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

    // Writes it now (a write that serializeLater() kept for the manga is dropped).
    void serialize(const QString& sourceName, const QString& mangaUrl);
    // Keeps it for flushPending(), deserialize() gets it meanwhile. For the page turns: the write is synced to the
    // disk, which takes a few ms on the device, the page would show up only after it. (On the gui thread.)
    void serializeLater(const QString& sourceName, const QString& mangaUrl);
    // Writes the ones that serializeLater() kept.
    static void flushPending();

    bool deserialize(const QString& sourceName, const QString& mangaUrl);

    // Finds the chapter by its url after the chapters changed. When it's gone (or there was no progress yet)
    // the start of the chapter at its index is taken. Returns whether the index changed.
    bool locate(const MangaChapterCollection& chapters);
};

QDataStream& operator<<(QDataStream& str, const ReadingProgress& m);
QDataStream& operator>>(QDataStream& str, ReadingProgress& m);
