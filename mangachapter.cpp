#include "mangachapter.h"

MangaChapter::MangaChapter()
    : chapterTitle(),
      chapterUrl(),
      pagesLoaded(false),
      numPages(0),
      pageUrlList(),
      imageUrlList()
{
}

MangaChapter::MangaChapter(const QString &title, const QString &url)
    : chapterTitle(title),
      chapterUrl(url),
      pagesLoaded(false),
      numPages(0),
      pageUrlList(),
      imageUrlList()
{
}

QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterUrl << m.chapterTitle << m.pagesLoaded << (qint32)m.numPages
        << m.pageUrlList << m.imageUrlList << (qint32)m.numPages
        << m.pageUrlList << m.imageUrlList;

    return str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{
    str >> m.chapterUrl >> m.chapterTitle >> m.pagesLoaded >> m.numPages >>
        m.pageUrlList >> m.imageUrlList >> m.numPages >> m.pageUrlList >>
        m.imageUrlList;
    ;

    return str;
}
