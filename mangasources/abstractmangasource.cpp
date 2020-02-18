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

    if (!QFile::exists(path))
        return path;

    DownloadFileJob *job = downloadmanager->downloadAsScaledImage(imagelink, path);

    if (job->await(5000))
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


QSharedPointer<MangaInfo> AbstractMangaSource::loadMangaInfo(const QString &mangalink, const QString &mangatitle, bool update)
{
    QFileInfo infofile(mangainfodir(name, mangatitle) + "mangainfo.dat");
    if (infofile.exists())
    {
        QSharedPointer<MangaInfo> mi(MangaInfo::deserialize(this->parent(), this, infofile.filePath()));
        if (update)
            mi->mangasource->updateMangaInfo(mi);
        return mi;
    }

    QSharedPointer<MangaInfo> mi(getMangaInfo(mangalink));
    mi->deserializeProgress();
    return mi;
}


void AbstractMangaSource::updateMangaInfo(QSharedPointer<MangaInfo> info)
{
    if (info.isNull() || info->updating)
        return;

    info->updating = true;

//    qDebug() << "updating" << info->title;

    if (!QFileInfo(info->coverpath).exists())
    {
        DownloadFileJob *cjob = AbstractMangaSource::downloadmanager->downloadAsFile(info->coverlink, info->coverpath, false);
        QObject::connect(cjob, SIGNAL(completed()), info.data(), SLOT(sendCoverLoaded()));
    }

    DownloadStringJob *job = downloadmanager->downloadAsString(info->link);

    //synchronously to prevent crashes
//    job->await();
//    updateMangaInfoFinishedLoading(job, info.data());
//    BindingClass(this, info, job).updateFinishedLoading();
    BindingClass *b = new BindingClass(this, info, job);

    QObject::connect(b, SIGNAL(completed(bool)), this, SLOT(updateMangaInfoReady(bool)));
}

void AbstractMangaSource::updateMangaInfoReady(bool downladsccessfull)
{
    BindingClass *b = static_cast<BindingClass *>(sender());

    if (downladsccessfull)
        updateMangaInfoFinishedLoading(b->job, b->mangainfo.data());
    b->mangainfo->updating = false;
    b->deleteLater();
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
