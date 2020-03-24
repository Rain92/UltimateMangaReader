#include "abstractmangasource.h"

#include <QDateTime>
#include <QImage>

#include "defines.h"
#include "mangainfo.h"

AbstractMangaSource::AbstractMangaSource(QObject *parent,
                                         DownloadManager *downloadmanager)
    : QObject(parent), downloadManager(downloadmanager), htmlConverter()
{
}

bool AbstractMangaSource::serializeMangaList()
{
    QFile file(mangalistdir + name + "_mangaList.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << mangaList.titles;
    out << mangaList.links;
    out << mangaList.absoluteUrls;
    out << mangaList.nominalSize;
    out << mangaList.actualSize;

    file.close();

    return true;
}

bool AbstractMangaSource::deserializeMangaList()
{
    mangaList.links.clear();
    mangaList.titles.clear();

    QFile file(mangalistdir + name + "_mangaList.dat");
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QDataStream in(&file);
    in >> mangaList.titles;
    in >> mangaList.links;
    in >> mangaList.absoluteUrls;
    in >> mangaList.nominalSize;
    in >> mangaList.actualSize;

    file.close();

    return true;
}

QString AbstractMangaSource::getImagePath(
    const DownloadImageDescriptor &descriptor)
{
    int ind = descriptor.imagelink.indexOf('?');
    if (ind == -1)
        ind = descriptor.imagelink.length();
    QString filetype = descriptor.imagelink.mid(ind - 4, 4);

    QString path = mangaimagesdir(name, descriptor.title) +
                   QString::number(descriptor.chapter) + "_" +
                   QString::number(descriptor.page) + filetype;

    return path;
}

QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadImage(
    const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    return downloadManager->downloadAsScaledImage(descriptor.imagelink, path);
}

QString AbstractMangaSource::downloadAwaitImage(
    const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    if (QFile::exists(path))
        return path;

    auto job =
        downloadManager->downloadAsScaledImage(descriptor.imagelink, path);

    return job->await(5000) ? path : "";
}

QSharedPointer<MangaInfo> AbstractMangaSource::loadMangaInfo(
    const QString &mangalink, const QString &mangatitle, bool update)
{
    QString path(mangainfodir(name, mangatitle) + "mangainfo.dat");
    if (QFile::exists(path) && true)
    {
        QSharedPointer<MangaInfo> mi(MangaInfo::deserialize(this, path));
        if (update)
        {
            mi->mangaSource->updateMangaInfo(mi);
            mi->serialize();
        }
        return mi;
    }
    else
    {
        QSharedPointer<MangaInfo> mi(getMangaInfo(mangalink));
        mi->serialize();

        return mi;
    }
}

QSharedPointer<MangaInfo> AbstractMangaSource::getMangaInfo(
    const QString &mangalink)
{
    auto job = downloadManager->downloadAsString(mangalink);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangaSource = this;
    info->hostname = name;

    info->link = mangalink;

    if (!job->await(3000))
        return info;

    updateMangaInfoFinishedLoading(job, info);

    downloadCover(info);

    return info;
}

void AbstractMangaSource::updateMangaInfo(QSharedPointer<MangaInfo> info)
{
    if (info->updating)
        return;

    info->updating = true;

    int oldnumchapters = info->chapters.count();

    auto job = downloadManager->downloadAsString(info->link);

    auto lambda = [oldnumchapters, info, job, this] {
        bool newchapters = info->numChapters > oldnumchapters;

        //        info->chapters.clear();
        //        info->chaperTitleListDescending.clear();
        //        info->numChapters = 0;

        updateMangaInfoFinishedLoading(job, info);

        info->updateCompeted(newchapters);

        downloadCover(info);
        info->serialize();
    };

    //    job->await(3000);
    //    lambda();

    executeOnJobCompletion(job, lambda);
}

bool AbstractMangaSource::updatePageList(QSharedPointer<MangaInfo> info,
                                         int chapter)
{
    if (chapter >= info->numChapters)
        return false;

    auto &ch = info->chapters[chapter];
    if (ch.pagesLoaded)
        return true;

    //    qDebug() << "getPageList start:" << chapterlink;

    auto newpagelist = getPageList(ch.chapterlink);
    ch.pagelinkList = newpagelist;
    if (ch.pagelinkList.count() == 0)
    {
        qDebug() << "pagelinks empty" << ch.chapterlink;
        ch.numPages = 1;
        ch.pagelinkList.clear();
        ch.pagelinkList << "";
        return false;
    }
    ch.numPages = ch.pagelinkList.count();
    ch.imagelinkList = QStringList();
    for (int i = 0; i < ch.pagelinkList.count(); i++)
        ch.imagelinkList.append("");
    ch.pagesLoaded = true;

    qDebug() << "getPageList finished:" << ch.chapterlink;
    return true;
}

void AbstractMangaSource::genrateCoverThumbnail(
    QSharedPointer<MangaInfo> mangainfo)
{
    QString scpath = mangainfo->coverThumbnailPath();

    if (!QFile::exists(scpath))
    {
        qDebug() << "generating scaled:" << mangainfo->title;
        QImage img;
        img.load(mangainfo->coverPath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(scpath);
    }
}

void AbstractMangaSource::downloadCover(QSharedPointer<MangaInfo> mangainfo)
{
    if (mangainfo->coverLink == "")
    {
        return;
    }

    if (mangainfo->coverPath == "")
    {
        int ind = mangainfo->coverLink.indexOf('?');
        if (ind == -1)
            ind = mangainfo->coverLink.length();
        QString filetype = mangainfo->coverLink.mid(ind - 4, 4);
        mangainfo->coverPath =
            mangainfodir(name, mangainfo->title) + "cover" + filetype;

        genrateCoverThumbnail(mangainfo);
    }

    auto coverjob = downloadManager->downloadAsFile(mangainfo->coverLink,
                                                    mangainfo->coverPath);

    auto lambda = [mangainfo]() { mangainfo->sendCoverLoaded(); };

    //    coverjob->await(1000);
    //    lambda();

    executeOnJobCompletion(coverjob, lambda);
}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlConverter.setHtml(str);
    return htmlConverter.toPlainText();
}

void AbstractMangaSource::fillMangaInfo(
    QSharedPointer<MangaInfo> info, const QString &buffer,
    const QRegularExpression &titlerx, const QRegularExpression &authorrx,
    const QRegularExpression &artistrx, const QRegularExpression &statusrx,
    const QRegularExpression &yearrx, const QRegularExpression &genresrx,
    const QRegularExpression &summaryrx, const QRegularExpression &coverrx)
{
    auto titlerxmatch = titlerx.match(buffer);
    auto authorrxmatch = authorrx.match(buffer);
    auto artistrxmatch = artistrx.match(buffer);
    auto statusrxmatch = statusrx.match(buffer);
    auto yearrxmatch = yearrx.match(buffer);
    auto genresrxmatch = genresrx.match(buffer);
    auto summaryrxmatch = summaryrx.match(buffer);
    auto coverrxmatch = coverrx.match(buffer);

    if (titlerxmatch.hasMatch())
        info->title = htmlToPlainText(titlerxmatch.captured(1)).trimmed();
    if (authorrxmatch.hasMatch())
        info->author = htmlToPlainText(authorrxmatch.captured(1)).remove('\n');
    if (artistrxmatch.hasMatch())
        info->artist = htmlToPlainText(artistrxmatch.captured(1)).remove('\n');
    if (statusrxmatch.hasMatch())
        info->status = htmlToPlainText(statusrxmatch.captured(1));
    if (yearrxmatch.hasMatch())
        info->releaseYear = htmlToPlainText(yearrxmatch.captured(1));
    if (genresrxmatch.hasMatch())
        info->genres = htmlToPlainText(genresrxmatch.captured(1))
                           .trimmed()
                           .remove('\n')
                           .remove(',');
    if (summaryrxmatch.hasMatch())
        info->summary = htmlToPlainText(summaryrxmatch.captured(1));
    if (coverrxmatch.hasMatch())
        info->coverLink = coverrxmatch.captured(1);
}
