#include "abstractmangasource.h"

#include <QDateTime>
#include <QtConcurrent/QtConcurrent>

#include "defines.h"
#include "mangainfo.h"

AbstractMangaSource::AbstractMangaSource(QObject *parent,
                                         DownloadManager *downloadmanager)
    : QObject(parent), downloadmanager(downloadmanager), htmlconverter()
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
    out << mangalist.absoluteUrls;
    out << mangalist.nominalSize;
    out << mangalist.actualSize;

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
    in >> mangalist.absoluteUrls;
    in >> mangalist.nominalSize;
    in >> mangalist.actualSize;

    file.close();

    return true;
}

QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadImage(
    const QString &imagelink, const QString &mangatitle, const int &chapternum,
    const int &pagenum)
{
    int ind = imagelink.indexOf('?');
    if (ind == -1)
        ind = imagelink.length();
    QString filetype = imagelink.mid(ind - 4, 4);
    QString path = mangaimagesdir(name, mangatitle) +
                   QString::number(chapternum) + "_" +
                   QString::number(pagenum) + filetype;

    return downloadmanager->downloadAsScaledImage(imagelink, path);
}

QString AbstractMangaSource::downloadAwaitImage(const QString &imagelink,
                                                const QString &mangatitle,
                                                const int &chapternum,
                                                const int &pagenum)
{
    int ind = imagelink.indexOf('?');
    if (ind == -1)
        ind = imagelink.length();
    QString filetype = imagelink.mid(ind - 4, 4);
    QString path = mangaimagesdir(name, mangatitle) +
                   QString::number(chapternum) + "_" +
                   QString::number(pagenum) + filetype;

    if (QFile::exists(path))
        return path;

    auto job = downloadmanager->downloadAsScaledImage(imagelink, path);

    return job->await(5000) ? path : "";
}

QSharedPointer<MangaInfo> AbstractMangaSource::loadMangaInfo(
    const QString &mangalink, const QString &mangatitle, bool update)
{
    QFileInfo infofile(mangainfodir(name, mangatitle) + "mangainfo.dat");
    if (infofile.exists())
    {
        QSharedPointer<MangaInfo> mi(
            MangaInfo::deserialize(this, infofile.filePath()));
        if (update)
            mi->mangasource->updateMangaInfo(mi);
        return mi;
    }
    else
    {
        QSharedPointer<MangaInfo> mi(getMangaInfo(mangalink));
        mi->deserializeProgress();
        mi->serialize();

        return mi;
    }
}

void AbstractMangaSource::updateMangaInfo(QSharedPointer<MangaInfo> info)
{
    if (info->updating)
        return;

    info->updating = true;

    int oldnumchapters = info->chapters.count();

    auto job = downloadmanager->downloadAsString(info->link);

    auto lambda = [oldnumchapters, info, this] {
        bool newchapters = info->numchapters > oldnumchapters;
        info->updateCompeted(newchapters);
        downloadCover(info);
        info->serialize();
    };

    //    job->await(3000);
    //    lambda();

    executeOnJobCompletion(job, lambda);
}

void AbstractMangaSource::downloadCover(QSharedPointer<MangaInfo> mangainfo)
{
    if (mangainfo->coverlink == "")
    {
        return;
    }

    if (mangainfo->coverpath == "")
    {
        int ind = mangainfo->coverlink.indexOf('?');
        if (ind == -1)
            ind = mangainfo->coverlink.length();
        QString filetype = mangainfo->coverlink.mid(ind - 4, 4);
        mangainfo->coverpath =
            mangainfodir(name, mangainfo->title) + "cover" + filetype;
    }

    auto coverjob = downloadmanager->downloadAsFile(mangainfo->coverlink,
                                                    mangainfo->coverpath);

    auto lambda = [mangainfo]() { mangainfo->sendCoverLoaded(); };

    //    coverjob->await(1000);
    //    lambda();

    executeOnJobCompletion(coverjob, lambda);
}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlconverter.setHtml(str);
    return htmlconverter.toPlainText();
}
