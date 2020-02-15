#include "mangachapter.h"
#include "configs.h"

MangaChapter::MangaChapter():
    chapterlink(),
    pagesloaded(false),
    numpages(0),
    pagelinks(),
    imagelinks(),
    source(nullptr)
{

}

MangaChapter::MangaChapter(const QString &link, AbstractMangaSource *source):
    chapterlink(link),
    pagesloaded(false),
    numpages(0),
    pagelinks(),
    imagelinks(),
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

    qDebug() << "getPageList start:" << chapterlink;

    pagelinks = source->getPageList(chapterlink);
    if (pagelinks.count() == 0)
    {
        qDebug() << "pagelinks empty" << chapterlink;
        numpages = 1;
        pagelinks.clear();
        pagelinks << "";
        return;
    }
    numpages = pagelinks.count();
    imagelinks = QStringList();
    for (int i = 0; i < pagelinks.count(); i++)
        imagelinks.append("");
    pagesloaded = true;
}


QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterlink << m.pagesloaded << (qint32)m.numpages << m.pagelinks << m.imagelinks << (qint32)m.numpages << m.pagelinks << m.imagelinks;

    return  str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{

    str >> m.chapterlink >> m.pagesloaded >> m.numpages >> m.pagelinks >> m.imagelinks >> m.numpages >> m.pagelinks >> m.imagelinks;;

    return str;
}

