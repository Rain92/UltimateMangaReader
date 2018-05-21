#include <QtCore>
#include <qdatetime.h>
#include <qstringlist.h>
#include "abstractmangasource.h"
#include "configs.h"


AbstractMangaSource::AbstractMangaSource(QObject *parent) :
    QObject(parent),
    nummangas(0)
{
}


bool AbstractMangaSource::serialize()
{
    QFile file(manglistcachdir + "/" + name + "_cache.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << mangalist.titles;
    out << mangalist.links;
    out << nummangas;

    file.close();

    return true;
}

bool AbstractMangaSource::deserialize()
{
    mangalist.links.clear();
    mangalist.titles.clear();

    QFile file(manglistcachdir + "/" + name + "_cache.dat");
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QDataStream in(&file);
    in >> mangalist.titles;
    in >> mangalist.links;
    in >> nummangas;

    file.close();

    return true;
}

DownloadFileJob *AbstractMangaSource::downloadImage(const QString &imagelink, const QString &mangatitle, const int &chapternum, const int &pagenum)
{
//    qDebug() << "downloadimage";
//    qDebug() << imagelink;
//    qDebug() << makeLegal(name + "_" + mangatitle);
//    qDebug() <<  QString::number(chapternum);
//    qDebug() << QString::number(pagenum);
//    qDebug() << imagelink.right(3);
    int ind = imagelink.indexOf('?');
    if (ind == -1)
        ind = imagelink.length();
    QString filetype = imagelink.mid(ind - 4, 4);
    QString path = downloaddirimages + "/" + makePathLegal(name + "_" + mangatitle) +
                   + "_" + QString::number(chapternum) + "_" + QString::number(pagenum) + filetype;



    DownloadFileJob *job = downloadmanager->downloadAsScaledImage(imagelink, path);

    return job;
}

QString AbstractMangaSource::downloadAwaitImage(const QString &imagelink, const QString &mangatitle, const int &chapternum, const int &pagenum)
{
    int ind = imagelink.indexOf('?');
    if (ind == -1)
        ind = imagelink.length();
    QString filetype = imagelink.mid(ind - 4, 4);
    QString path = downloaddirimages + "/" + makePathLegal(name + "_" + mangatitle) +
                   + "_" + QString::number(chapternum) + "_" + QString::number(pagenum) + filetype;

    QFileInfo completedfile (path + ".completed");

    DownloadFileJob *job = downloadmanager->downloadAsScaledImage(imagelink, path);


    if (completedfile.exists() || job->await(10000))
    {
        downloadmanager->fileDownloads->remove(imagelink);
        delete job;
        return path;
    }
    else
    {
        downloadmanager->fileDownloads->remove(imagelink);
        delete job;
        return "";
    }
}

