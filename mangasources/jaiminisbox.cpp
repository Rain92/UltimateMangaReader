#include "jaiminisbox.h"

JaiminisBox::JaiminisBox(DownloadManager *dm) : AbstractMangaSource(dm)
{
    name = "JaiminisBox";
    baseurl = "https://jaiminisbox.com/";

    QUrlQuery postdata;
    postdata.addQueryItem("adult", "true");
    mangaInfoPostDataStr = postdata.query().toUtf8();
}

bool JaiminisBox::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<div class="title"><a href="([^"]*)"[^"]*"([^"]*)")lit");

    QRegularExpression numpagesrx(R"(/directory/(\d+)/">Last)");

    MangaList mangas;
    mangas.absoluteUrls = true;

    QString readerlink = baseurl + "reader/directory/";

    auto job = downloadManager->downloadAsString(readerlink + "1", -1);

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto numpagesrxmatch = numpagesrx.match(job->buffer);

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    qDebug() << "pages" << pages;
    const int matchesPerPage = 36;

    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer))
        {
            mangas.links.append(match.captured(1));
            mangas.titles.append(htmlToPlainText(match.captured(2)).trimmed());
            matches++;
        }
        mangas.size += matches;

        token->sendProgress(10 + 90 * (mangas.size / matchesPerPage) / pages);

        qDebug() << "matches:" << matches;
    };

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(readerlink + QString::number(i));

    DownloadQueue queue(downloadManager, urls, CONF.parallelDownloadsLow, lambda, true);
    queue.setCancellationToken(&token->canceled);
    queue.start();
    if (!queue.awaitCompletion())
    {
        token->sendError(queue.lastErrorMessage);
        return false;
    }
    this->mangaList = mangas;

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

void JaiminisBox::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                                 QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<h1 class="title">\s+([^<]*))");
    QRegularExpression authorrx("Author</b>:([^<]*)");
    QRegularExpression artistrx("Artist</b>:([^<]*)");
    QRegularExpression statusrx;
    QRegularExpression yearrx;
    QRegularExpression genresrx;
    QRegularExpression summaryrx("Synopsis</b>:(.*)</div>");
    QRegularExpression coverrx(R"(<div class="thumbnail">[^"]*"([^"]*))");

    QRegularExpression chapterrx("<div class=\"title\"><a href=\"([^\"]*)\"\\s+title=\"([^\"]*)");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx,
                  coverrx);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> JaiminisBox::getPageList(const QString &chapterlink)
{
    QRegularExpression encodedrx(R"(JSON.parse\(atob\("([^"]*))");
    QRegularExpression imagelinksrx(R"("url":"([^"]*))");

    auto job = downloadManager->downloadAsString(chapterlink, 6000, mangaInfoPostDataStr);

    if (!job->await(7000))
        return Err(job->errorString);

    auto rxmatch = encodedrx.match(job->buffer);
    if (!rxmatch.hasMatch())
        return Err(QString("Error. Couldn't process pages/images."));

    QByteArray decoded = QByteArray::fromBase64(rxmatch.captured(1).toLatin1());
    QString decodedstr(decoded);

    QStringList imageLinks;
    for (auto &match : getAllRxMatches(imagelinksrx, decodedstr))
    {
        imageLinks.append(match.captured(1).replace("\\/", "/"));
    }

    return Ok(imageLinks);
}
