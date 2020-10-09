#include "abstractmangasource.h"

#include "mangainfo.h"

AbstractMangaSource::AbstractMangaSource(NetworkManager *networkManager)
    : mangaInfoPostDataStr(), networkManager(networkManager), htmlConverter()
{
}

bool AbstractMangaSource::serializeMangaList()
{
    QFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QDataStream out(&file);
    out << mangaList;
    file.close();

    return true;
}

bool AbstractMangaSource::deserializeMangaList()
{
    QFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    in >> mangaList;
    file.close();

    return true;
}

QString AbstractMangaSource::getImagePath(const DownloadImageDescriptor &descriptor)
{
    // save all images as jpg
    QString filetype = ".jpg";

    QString path = CONF.mangaimagesdir(name, descriptor.title) + QString::number(descriptor.chapter) + "_" +
                   QString::number(descriptor.page) + filetype;

    return path;
}

QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadImage(const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    return networkManager->downloadAsScaledImage(descriptor.imageUrl, path);
}

Result<QString, QString> AbstractMangaSource::downloadAwaitImage(const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    if (QFile::exists(path))
        return Ok(path);

    auto job = networkManager->downloadAsScaledImage(descriptor.imageUrl, path);

    if (job->await(7000))
        return Ok(path);
    else
        return Err(job->errorString);
}

Result<QString, QString> AbstractMangaSource::getImageUrl(const QString &pageurl)
{
    // Default implementation:
    // pageurls are actually already imageurls
    return Ok(pageurl);
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::loadMangaInfo(const QString &mangaUrl,
                                                                              const QString &mangaTitle,
                                                                              bool update)
{
    QString path(CONF.mangainfodir(name, mangaTitle) + "mangainfo.dat");
    if (QFile::exists(path))
    {
        try
        {
            auto info = MangaInfo::deserialize(this, path);
            if (update)
                info->mangaSource->updateMangaInfoAsync(info);

            return Ok(info);
        }
        catch (QException)
        {
        }
    }

    auto infoR = getMangaInfo(mangaUrl, mangaTitle);

    if (infoR.isOk())
        infoR.unwrap()->serialize();

    return infoR;
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::getMangaInfo(const QString &mangaUrl,
                                                                             const QString &mangaTitle)
{
    auto job = networkManager->downloadAsString(mangaUrl, 8000, mangaInfoPostDataStr);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangaSource = this;
    info->hostname = name;
    info->url = mangaUrl;
    info->title = mangaTitle;

    if (!job->await(8000))
        return Err(job->errorString);

    updateMangaInfoFinishedLoading(job, info);

    downloadCoverAsync(info);

    return Ok(info);
}

void AbstractMangaSource::updateMangaInfoAsync(QSharedPointer<MangaInfo> info)
{
    int oldnumchapters = info->chapters.count();

    auto job = networkManager->downloadAsString(info->url, 8000, mangaInfoPostDataStr);

    auto lambda = [oldnumchapters, info, job, this] {
        {
            QMutexLocker locker(info->updateMutex.get());
            updateMangaInfoFinishedLoading(job, info);
        }

        bool newchapters = info->chapters.count() > oldnumchapters;
        info->updateCompeted(newchapters);

        downloadCoverAsync(info);
        info->serialize();
    };

    executeOnJobCompletion(job, lambda);
}

Result<void, QString> AbstractMangaSource::updatePageList(QSharedPointer<MangaInfo> info, int chapter)
{
    if (chapter >= info->chapters.count() || chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    if (info->chapters[chapter].pagesLoaded)
        return Ok();

    auto newpagelistR = getPageList(info->chapters[chapter].chapterUrl);

    if (!newpagelistR.isOk())
        return Err(newpagelistR.unwrapErr());

    auto newpagelist = newpagelistR.unwrap();

    QMutexLocker locker(info->updateMutex.get());

    if (chapter >= info->chapters.count() || chapter < 0)
        return Err(QString("Chapter number out of bounds."));
    auto &ch = info->chapters[chapter];

    ch.pageUrlList = newpagelist;

    if (ch.pageUrlList.count() == 0)
    {
        qDebug() << "pageUrls empty" << ch.chapterUrl;
        ch.pageUrlList.clear();
        ch.pageUrlList << "";
        return Err(QString("Can't download chapter: pagelist empty."));
    }
    ch.imageUrlList = QStringList();
    for (int i = 0; i < ch.pageUrlList.count(); i++)
        ch.imageUrlList.append("");
    ch.pagesLoaded = true;

    return Ok();
}

void AbstractMangaSource::genrateCoverThumbnail(QSharedPointer<MangaInfo> mangainfo)
{
    QString scpath = mangainfo->coverThumbnailPath();

    if (!QFile::exists(scpath))
    {
        QImage img;
        img.load(mangainfo->coverPath);
        img = img.scaled(SIZES.favoriteCoverSize * qApp->devicePixelRatio(),
                         SIZES.favoriteCoverSize * qApp->devicePixelRatio(), Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
        img.save(scpath);
    }
}

void AbstractMangaSource::downloadCoverAsync(QSharedPointer<MangaInfo> mangainfo)
{
    if (mangainfo->coverUrl == "")
    {
        return;
    }

    if (mangainfo->coverPath == "")
    {
        int ind = mangainfo->coverUrl.indexOf('?');
        if (ind == -1)
            ind = mangainfo->coverUrl.length();
        QString filetype = mangainfo->coverUrl.mid(ind - 4, 4);
        mangainfo->coverPath = CONF.mangainfodir(name, mangainfo->title) + "cover" + filetype;
    }

    auto coverjob = networkManager->downloadAsFile(mangainfo->coverUrl, mangainfo->coverPath);

    auto lambda = [this, mangainfo]() {
        genrateCoverThumbnail(mangainfo);
        mangainfo->sendCoverLoaded();
    };

    executeOnJobCompletion(coverjob, lambda);
}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlConverter.setHtml(str);
    return htmlConverter.toPlainText();
}

void AbstractMangaSource::fillMangaInfo(QSharedPointer<MangaInfo> info, const QString &buffer,
                                        const QRegularExpression &authorrx,
                                        const QRegularExpression &artistrx,
                                        const QRegularExpression &statusrx, const QRegularExpression &yearrx,
                                        const QRegularExpression &genresrx,
                                        const QRegularExpression &summaryrx,
                                        const QRegularExpression &coverrx)
{
    auto authorrxmatch = authorrx.match(buffer);
    auto artistrxmatch = artistrx.match(buffer);
    auto statusrxmatch = statusrx.match(buffer);
    auto yearrxmatch = yearrx.match(buffer);
    auto genresrxmatch = genresrx.match(buffer);
    auto summaryrxmatch = summaryrx.match(buffer);
    auto coverrxmatch = coverrx.match(buffer);

    QRegularExpression bbrx(R"(\[.*?\])");

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
                           .replace(", ", " ")
                           .replace(",", " ");
    if (summaryrxmatch.hasMatch())
        info->summary = htmlToPlainText(summaryrxmatch.captured(1)).remove(bbrx);
    if (coverrxmatch.hasMatch())
        info->coverUrl = coverrxmatch.captured(1);
}
