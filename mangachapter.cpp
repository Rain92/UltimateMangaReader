#include "mangachapter.h"


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

MangaChapter::~MangaChapter()
{
    if (pagesloaded)
        delete pagelinks;
}

void MangaChapter::loadPages()
{
    if (pagesloaded)
        return;

    pagelinks = source->getPageList(chapterlink);
    numpages = pagelinks->count();
    imagelinks = QVector<QString>(numpages);
    pagesloaded = true;
}


QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterlink << m.pagesloaded;

    if (m.pagesloaded)
        return str  << (qint32)m.numpages << *m.pagelinks << m.imagelinks;

    return  str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{
    if (m.pagesloaded)
        delete m.pagelinks;

    str >> m.chapterlink >> m.pagesloaded;

    if (!m.pagesloaded)
        return str;

    QVector<QString> tmppagelinks;
    str >> m.numpages >> tmppagelinks >> m.imagelinks;
    m.pagelinks = new QVector<QString>(tmppagelinks);

    return str;
}

