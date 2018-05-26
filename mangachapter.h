#ifndef MANGACHAPTER_H
#define MANGACHAPTER_H

#include "abstractmangasource.h"

class MangaChapter
{
public:
    QString chapterlink;

    bool pagesloaded;
    int numpages;
    QVector<QString> *pagelinks;
    QVector<QString> imagelinks;

    MangaChapter(const QString &link, AbstractMangaSource *source);
    MangaChapter();
    ~MangaChapter();

    void loadPages();

//private:
    AbstractMangaSource *source;
};


QDataStream &operator<<(QDataStream &str, const MangaChapter &m);
QDataStream &operator>>(QDataStream &str, MangaChapter &m);


#endif // MANGACHAPTER_H
