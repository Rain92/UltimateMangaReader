#include "readingstate.h"


ReadingState::ReadingState():
    currentindex(0, 0),
    maxindex(0, 0),
    coverpath(""),
    mangalink(""),
    isfavorite(false),
    updated(false)
{}

ReadingState::ReadingState(QString hostname, QString title, qint32 oldnumchapters,
                           MangaIndex currentindex, MangaIndex maxindex, QString coverpath, QString mangalink) :
    hostname(hostname),
    title(title),
    oldnumchapters(oldnumchapters),
    currentindex(currentindex),
    maxindex(maxindex),
    coverpath(coverpath),
    mangalink(mangalink),
  isfavorite(false),
  updated(false)
{}


QString ReadingState::getSerializationFilename() const
{
    return readingstatesdir + "/" + makePathLegal(hostname + "_" + title) + ".dat";
}


QString ReadingState::getKey()
{
    return hostname + "_" + title;
}


QDataStream &operator<<(QDataStream &str, const ReadingState &m)
{
    str << m.title << m.hostname << (qint32)m.oldnumchapters << m.currentindex << m.maxindex << m.isfavorite;
    if (m.isfavorite)
        str << m.coverpath << m.mangalink;
    return str;
}

QDataStream &operator>>(QDataStream &str, ReadingState &m)
{
    str >> m.title >> m.hostname >> m.oldnumchapters >> m.currentindex >> m.maxindex >> m.isfavorite;
    if (m.isfavorite)
        str >> m.coverpath >> m.mangalink;
    return str;
}


