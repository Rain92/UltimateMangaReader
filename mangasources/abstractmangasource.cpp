#include "abstractmangasource.h"

#include <QDateTime>
#include <QtConcurrent/QtConcurrent>

#include "configs.h"
#include "mangainfo.h"

AbstractMangaSource::AbstractMangaSource(QObject *parent,
                                         DownloadManager *downloadmanager)
    : QObject(parent),
      nummangas(0),
      downloadmanager(downloadmanager),
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
            MangaInfo::deserialize(this->parent(), this, infofile.filePath()));
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

    QSharedPointer<DownloadFileJob> cjob(nullptr);

    if (!QFileInfo::exists(info->coverpath))
    {
        cjob = AbstractMangaSource::downloadmanager->downloadAsFile(
            info->coverlink, info->coverpath);

        QObject::connect(cjob.get(), SIGNAL(completed()), info.get(),
                         SLOT(sendCoverLoaded()));
    }

    auto job = downloadmanager->downloadAsString(info->link);

    QtConcurrent::run([info, job, cjob, this]() {
        job->await(2000, true);

        if (cjob)
            cjob->await(2000);

        updateMangaInfoFinishedLoading(job, info);
    });
}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlconverter.setHtml(str);
    return htmlconverter.toPlainText();
}
