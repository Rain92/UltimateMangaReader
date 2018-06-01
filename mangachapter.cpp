#include "mangachapter.h"
#include "configs.h"

MangaChapter::MangaChapter():
    pagesloaded(false),
    source(nullptr)
{

}

MangaChapter::MangaChapter(const QString &link, AbstractMangaSource *source):
    chapterlink(link),
    pagesloaded(false),
    source(source)
{

}


void MangaChapter::loadPages()
{
    if (pagesloaded)
        return;

    pagelinks = source->getPageList(chapterlink);
    numpages = pagelinks.count();
    imagelinks = QStringList();
    for (int i = 0; i < pagelinks.count(); i++)
        imagelinks.append("");
    pagesloaded = true;
}


QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterlink << m.pagesloaded;

    if (m.pagesloaded)
        return str  << (qint32)m.numpages << m.pagelinks << m.imagelinks;

    return  str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{

    str >> m.chapterlink >> m.pagesloaded;

    if (!m.pagesloaded)
        return str;

    str >> m.numpages >> m.pagelinks >> m.imagelinks;

    return str;
}

