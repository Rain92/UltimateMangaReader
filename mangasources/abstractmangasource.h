#ifndef ABSTRACTMANGASOURCE_H
#define ABSTRACTMANGASOURCE_H
#include <QtCore>
#include <qdatetime.h>
#include <qstringlist.h>
#include "downloadmanager.h"
//#include "mangainfo.h"

class MangaInfo;

struct MangaList
{
    QStringList titles;
    QStringList links;
};

class AbstractMangaSource : public QObject
{
    Q_OBJECT
public:
    QString name;

    QString baseurl;

    MangaList mangalist;
    int nummangas;

    AbstractMangaSource(QObject *parent);


    virtual bool updateMangaList() = 0;

//    virtual int *getNumChapters(QString mangalink) = 0;

    virtual MangaInfo *getMangaInfo(QString mangalink) = 0;
//    virtual MangaInfo *getMangaInfoFrom(QString mangalink) = 0;

    virtual QStringList *getPageList(const QString &chapterlink) = 0;
    virtual QString getImageLink(const QString &pagelink) = 0;


    bool serializeMangaList();
    bool deserializeMangaList();

    DownloadFileJob *downloadImage(const QString &imagelink, const QString &mangatitle,
                                   const int &chapternum, const int &pagenum);
    QString downloadAwaitImage(const QString &imagelink, const QString &mangatitle,
                               const int &chapternum, const int &pagenum);

signals:
    void  progress(int);

protected:
    DownloadManager *downloadmanager;

};




#endif // ABSTRACTMANGASOURCE_H
