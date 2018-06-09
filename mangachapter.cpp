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

MangaChapter::MangaChapter(const MangaChapter &origin):
    chapterlink(origin.chapterlink),
    pagesloaded(origin.pagesloaded),
    numpages(origin.numpages),
    pagelinks(origin.pagelinks),
    imagelinks(origin.imagelinks),
    source(origin.source)
{
//    qDebug() << "copyconstuctor";
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

