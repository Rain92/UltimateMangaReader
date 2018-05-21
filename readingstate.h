#ifndef READINGSTATE_H
#define READINGSTATE_H

#include "configs.h"
#include "mangaindex.h"
#include "mangainfo.h"

struct ReadingState
{
    QString hostname;
    QString title;
    int oldnumchapters;
    MangaIndex currentindex;
    MangaIndex maxindex;
    QString coverpath;
    QString mangalink;
    bool isfavorite;
    bool updated;


    ReadingState();

    ReadingState(
        QString hostname,
        QString title,
        qint32 oldnumchapters,
        MangaIndex currentindex,
        MangaIndex maxindex,
        QString coverpath,
        QString mangalink);

    QString getSerializationFilename() const;


    QString getKey();


    static ReadingState fromMangaInfo(const MangaInfo &info)
    {
        return ReadingState(info.hostname, info.title, info.numchapters, info.currentindex, info.currentindex, info.coverpath, info.link);
    }
};



QDataStream &operator<<(QDataStream &str, const ReadingState &m);

QDataStream &operator>>(QDataStream &str, ReadingState &m);


#endif // READINGSTATE_H
