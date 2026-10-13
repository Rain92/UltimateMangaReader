#include "mangachapter.h"

MangaChapter::MangaChapter() : chapterTitle(), chapterUrl(), pagesLoaded(false), pages() {}

MangaChapter::MangaChapter(const QString &title, const QString &url)
    : chapterTitle(title), chapterUrl(url), pagesLoaded(false), pages()
{
}

QDataStream &operator<<(QDataStream &str, const MangaPage &m)
{
    return str << m.link << m.image;
}

QDataStream &operator>>(QDataStream &str, MangaPage &m)
{
    return str >> m.link >> m.image;
}

QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    return str << m.chapterTitle << m.chapterUrl << m.pagesLoaded << m.pages;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{
    return str >> m.chapterTitle >> m.chapterUrl >> m.pagesLoaded >> m.pages;
}
