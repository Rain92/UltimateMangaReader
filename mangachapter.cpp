#include "mangachapter.h"

#include "defines.h"

MangaChapter::MangaChapter()
    : chapterlink(),
      pagesLoaded(false),
      numPages(0),
      pagelinkList(),
      imagelinkList(),
      source(nullptr)
{
}

MangaChapter::MangaChapter(const QString &link, AbstractMangaSource *source)
    : chapterlink(link),
      pagesLoaded(false),
      numPages(0),
      pagelinkList(),
      imagelinkList(),
      source(source)
{
}

void MangaChapter::loadPages()
{
    if (pagesLoaded)
        return;

    qDebug() << "getPageList start:" << chapterlink;

    pagelinkList = source->getPageList(chapterlink);
    if (pagelinkList.count() == 0)
    {
        qDebug() << "pagelinks empty" << chapterlink;
        numPages = 1;
        pagelinkList.clear();
        pagelinkList << "";
        return;
    }
    numPages = pagelinkList.count();
    imagelinkList = QStringList();
    for (int i = 0; i < pagelinkList.count(); i++)
        imagelinkList.append("");
    pagesLoaded = true;
}

QDataStream &operator<<(QDataStream &str, const MangaChapter &m)
{
    str << m.chapterlink << m.pagesLoaded << (qint32)m.numPages << m.pagelinkList
        << m.imagelinkList << (qint32)m.numPages << m.pagelinkList << m.imagelinkList;

    return str;
}

QDataStream &operator>>(QDataStream &str, MangaChapter &m)
{
    str >> m.chapterlink >> m.pagesLoaded >> m.numPages >> m.pagelinkList >>
        m.imagelinkList >> m.numPages >> m.pagelinkList >> m.imagelinkList;
    ;

    return str;
}
