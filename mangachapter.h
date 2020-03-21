#ifndef MANGACHAPTER_H
#define MANGACHAPTER_H

#include "abstractmangasource.h"

class MangaChapter
{
public:
    QString chapterlink;

    bool pagesLoaded;
    int numPages;

    QStringList pagelinkList;
    QStringList imagelinkList;

    MangaChapter(const QString &link, AbstractMangaSource *source);
    MangaChapter();

    void loadPages();

    // private:
    AbstractMangaSource *source;
};

QDataStream &operator<<(QDataStream &str, const MangaChapter &m);
QDataStream &operator>>(QDataStream &str, MangaChapter &m);

#endif  // MANGACHAPTER_H
