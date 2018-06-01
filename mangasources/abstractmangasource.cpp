#include <QtCore>
#include <qdatetime.h>
#include <qstringlist.h>
#include "abstractmangasource.h"
#include "configs.h"
#include "mangainfo.h"


AbstractMangaSource::AbstractMangaSource(QObject *parent) :
    QObject(parent),
    nummangas(0),
    htmlconverter()
{
}


bool AbstractMangaSource::serializeMangaList()
{
    QFile file(mangalistdir + name + "_mangalist.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << mangalist.titles;
    out << mangalist.links;
    out << (qint32)nummangas;

    file.close();

    return true;
}

bool AbstractMangaSource::deserializeMangaList()
{
    mangalist.links.clear();
    mangalist.titles.clear();

    QFile file(mangalistdir + name + "_mangalist.dat");
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
    int ind = imagelink.indexOf('?');
    if (ind == -1)
        ind = imagelink.length();
    QString filetype = imagelink.mid(ind - 4, 4);
    QString path = mangaimagesdir(name, mangatitle) + QString::number(chapternum) + "_" + QString::number(pagenum) + filetype;





    DownloadFileJob *job = downloadmanager->downloadAsScaledImage(imagelink, path);

    return job;
}

QString AbstractMangaSource::downloadAwaitImage(const QString &imagelink, const QString &mangatitle, const int &chapternum, const int &pagenum)
{
    int ind = imagelink.indexOf('?');
    if (ind == -1)
        ind = imagelink.length();
    QString filetype = imagelink.mid(ind - 4, 4);
    QString path = mangaimagesdir(name, mangatitle) + QString::number(chapternum) + "_" + QString::number(pagenum) + filetype;

    QFileInfo completedfile (path + ".completed");

    DownloadFileJob *job = downloadmanager->downloadAsScaledImage(imagelink, path);


    if (completedfile.exists() || job->await(5000))
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


MangaInfo *AbstractMangaSource::loadMangaInfo(const QString &mangalink, const QString &mangatitle)
{
    QFileInfo infofile(mangainfodir(name, mangatitle) + "mangainfo.dat");
    if (infofile.exists())
    {
        MangaInfo *mi = MangaInfo::deserialize(this->parent(), this, infofile.filePath());
        mi->mangasource->updateMangaInfo(mi);
        return mi;
    }

    return getMangaInfo(mangalink);
}


void AbstractMangaSource::updateMangaInfo(MangaInfo *info)
{
    DownloadStringJob *job = downloadmanager->downloadAsString(info->link);

    QObject::connect(job, SIGNAL(completed()), new BindingClass(this, info, job), SLOT(updateFinishedLoading()));

}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlconverter.setHtml(str);
    return htmlconverter.toPlainText();
}

//AbstractMangaSource *AbstractMangaSource::getSourceByName(const QString &name)
//{
//    foreach (AbstractMangaSource *s, sources)
//        if (s->name == name)
//            return s;
//    return nullptr;

//}
