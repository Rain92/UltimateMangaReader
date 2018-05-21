#ifndef MANGACHAPTER_H
#define MANGACHAPTER_H

#include "abstractmangasource.h"

class MangaChapter
{
public:
    QString chapterlink;

    bool pagesloaded;
    int numpages;
    QStringList *pagelinks;
    QVector<QString> imagelinks;

    MangaChapter(const QString &link, AbstractMangaSource *source);
    ~MangaChapter();

    void loadPages();

private:
    AbstractMangaSource *source;
};

#endif // MANGACHAPTER_H
