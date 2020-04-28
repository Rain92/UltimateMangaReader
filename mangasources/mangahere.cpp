#include "mangahere.h"

MangaHere::MangaHere(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "MangaHere";
    baseurl = "https://www.mangahere.cc";
    dictionaryUrl = "https://www.mangahere.cc/directory/1/";

    networkManager->addCookie(".mangahere.cc", "isAdult", "1");
}

bool MangaHere::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit("(/manga/[^"]*)" title="((?:.(?!><))*)">\s*<img)lit");

    QRegularExpression numpagesrx(R"(\.\.\.</span><a[^>]*>(\d*)<)");

    auto job = networkManager->downloadAsString(dictionaryUrl + "1.htm");

    if (!job->await())
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto numpagesrxmatch = numpagesrx.match(job->buffer);

    MangaList mangas;
    mangas.absoluteUrls = false;

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();
    //    pages = 1;
    qDebug() << "pages:" << pages;

    const int matchesPerPage = 70;
    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer))
        {
            mangas.urls.append(match.captured(1));
            mangas.titles.append(htmlToPlainText(htmlToPlainText(match.captured(2))));
            matches++;
        }
        mangas.size += matches;

        token->sendProgress(10 + 90 * (mangas.size / matchesPerPage) / pages);
        qDebug() << "matches:" << matches;
    };

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(dictionaryUrl + QString::number(i) + ".htm");

    DownloadQueue queue(networkManager, urls, CONF.parallelDownloadsHigh, lambda, true);
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

void MangaHere::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                               QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"lit(<meta name="og:title" content="([^"]*?)")lit");
    QRegularExpression authorrx(R"lit(<a href="/search/author/[^+][^"]*"[^>]*?title="([^"]*)">)lit");
    QRegularExpression artistrx;
    QRegularExpression statusrx(R"(<span class="detail-info-right-title-tip">([^<]*?)<)");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(<p class="detail-info-right-tag-list">(.*?)</p>)");

    QRegularExpression summaryrx(R"lit(<p[^>]*?class="fullcontent">(.*?)</p>)lit");

    QRegularExpression coverrx(R"lit(<img class="detail-info-cover-img" src="([^"]*?)")lit");

    QRegularExpression chapterrx(
        R"lit(<a href="(/manga/[^"]*?)" title=".*?<p class="title3">([^<]*?)</p>)lit");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx,
                  coverrx);

    info->genres = info->genres.remove("- ");
    info->status = info->status.remove('\n');
    if (info->status.contains('-'))
        info->status = info->status.split('-')[0];

    int spos = job->buffer.indexOf(R"(<div id="chapterlist">)");
    int epos = job->buffer.indexOf(R"(<div class="fb-comments)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), baseurl + chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

// UNFINISHED
Result<QStringList, QString> MangaHere::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit( TODO )lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    int spos = job->buffer.indexOf(R"(<div class="vung-doc" id="vungdoc">)");
    int epos = job->buffer.indexOf(R"(class="navi-change-chapter">)", spos);

    QStringList imageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->buffer, spos, epos))
    {
        auto imageUrl = match.captured(1);
        if (!imageUrl.contains("/themes/"))
            imageUrls.append(imageUrl);
    }

    return Ok(imageUrls);
}
