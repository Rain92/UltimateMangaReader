#include "mangachapter.h"

MangaChapter::MangaChapter()
    : chapterTitle(), chapterUrl(), pagesLoaded(false), numPages(0), pageUrlList(), imageUrlList()
{
}

MangaChapter::MangaChapter(const QString &title, const QString &url)
    : chapterTitle(title), chapterUrl(url), pagesLoaded(false), numPages(0), pageUrlList(), imageUrlList()
{
}

QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterTitle << m.chapterUrl << m.pagesLoaded << (qint32)m.numPages << m.pageUrlList
        << m.imageUrlList << m.chapterNumber;

    return str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{
    str >> m.chapterTitle >> m.chapterUrl >> m.pagesLoaded >> m.numPages >> m.pageUrlList >> m.imageUrlList >>
        m.chapterNumber;

    return str;
}
