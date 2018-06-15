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

    virtual void updateMangaInfoFinishedLoading(DownloadStringJob *job, MangaInfo *mangainfo) = 0;

    virtual MangaInfo *getMangaInfo(QString mangalink) = 0;

    virtual QStringList getPageList(const QString &chapterlink) = 0;
    virtual QString getImageLink(const QString &pagelink) = 0;

    QSharedPointer<MangaInfo> loadMangaInfo(const QString &mangalink, const QString &mangatitle, bool update = true);


    bool serializeMangaList();
    bool deserializeMangaList();

    DownloadFileJob *downloadImage(const QString &imagelink, const QString &mangatitle,
                                   const int &chapternum, const int &pagenum);
    QString downloadAwaitImage(const QString &imagelink, const QString &mangatitle,
                               const int &chapternum, const int &pagenum);


//    static QList<AbstractMangaSource *> sources;
//    static AbstractMangaSource *getSourceByName(const QString &name);


    QString htmlToPlainText(const QString &str);

public slots:
    void updateMangaInfo(QSharedPointer<MangaInfo> mangainfo);

private slots:
    void updateMangaInfoReady();

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
        job(job)
    {
        QObject::connect(job, SIGNAL(completed()), this, SLOT(updateFinishedLoading()));
        QObject::connect(job, SIGNAL(downloadError()), this, SLOT(error()));
    }
    ~BindingClass()
    {
        delete job;
    }

public slots:
    void updateFinishedLoading()
    {
        emit completed();
//        mangasource->updateMangaInfoFinishedLoading(job, mangainfo.data());
//        mangainfo.clear();
//        job->deleteLater();
//        this->deleteLater();
    }
private slots:
    void error()
    {
        qDebug() << "binding class: download error";
//        delete job;
//        this->deleteLater();
    }

signals:
    void completed();

public:
    AbstractMangaSource *mangasource;
    QSharedPointer<MangaInfo> mangainfo;
    DownloadStringJob *job;
};



#endif // ABSTRACTMANGASOURCE_H
