#ifndef ABSTRACTMANGASOURCE_H
#define ABSTRACTMANGASOURCE_H
#include <QtCore>
#include <qdatetime.h>
#include <qstringlist.h>
#include <QTextDocument>
#include "downloadmanager.h"

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

    virtual void initialize() {}

    virtual bool updateMangaList() = 0;


    virtual MangaInfo *getMangaInfo(QString mangalink) = 0;

    virtual void updateMangaInfoFinishedLoading(DownloadStringJob *job, MangaInfo *mangainfo) = 0;

    virtual QStringList getPageList(const QString &chapterlink) = 0;
    virtual QString getImageLink(const QString &pagelink) = 0;

    QSharedPointer<MangaInfo> loadMangaInfo(const QString &mangalink, const QString &mangatitle);

    void updateMangaInfo(QSharedPointer<MangaInfo> mangainfo);

    bool serializeMangaList();
    bool deserializeMangaList();

    DownloadFileJob *downloadImage(const QString &imagelink, const QString &mangatitle,
                                   const int &chapternum, const int &pagenum);
    QString downloadAwaitImage(const QString &imagelink, const QString &mangatitle,
                               const int &chapternum, const int &pagenum);


//    static QList<AbstractMangaSource *> sources;
//    static AbstractMangaSource *getSourceByName(const QString &name);


    QString htmlToPlainText(const QString &str);

signals:
    void  updateProgress(int);
    void  updateError(QString);

protected:
    DownloadManager *downloadmanager;
    QTextDocument htmlconverter;


};

class BindingClass : public QObject
{
    Q_OBJECT
public:
    BindingClass(
        AbstractMangaSource *mangasource,
        QSharedPointer<MangaInfo> mangainfo,
        DownloadStringJob *job):
        mangasource(mangasource),
        mangainfo(mangainfo),
        job(job) {}

public slots:
    void updateFinishedLoading()
    {
        mangasource->updateMangaInfoFinishedLoading(job, mangainfo.data());
        this->deleteLater();
    }

private:
    AbstractMangaSource *mangasource;
    QSharedPointer<MangaInfo> mangainfo;
    DownloadStringJob *job;
};



#endif // ABSTRACTMANGASOURCE_H
