#include "mangachapter.h"


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
