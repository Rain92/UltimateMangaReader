#include "mangachapter.h"

#include "defines.h"

MangaChapter::MangaChapter()
    : chapterlink(),
      pagesLoaded(false),
      numPages(0),
      pagelinkList(),
      imagelinkList()
{
}

MangaChapter::MangaChapter(const QString &link)
    : chapterlink(link),
      pagesLoaded(false),
      numPages(0),
      pagelinkList(),
      imagelinkList()
{
}

QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterlink << m.pagesLoaded << (qint32)m.numPages
        << m.pagelinkList << m.imagelinkList << (qint32)m.numPages
        << m.pagelinkList << m.imagelinkList;

    return str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{
    str >> m.chapterlink >> m.pagesLoaded >> m.numPages >> m.pagelinkList >>
        m.imagelinkList >> m.numPages >> m.pagelinkList >> m.imagelinkList;
    ;

    return str;
}
