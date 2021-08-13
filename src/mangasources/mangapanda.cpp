#include "mangapanda.h"

MangaPanda::MangaPanda(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaPanda";
    baseUrl = "http://manga-panda.xyz/";
    dictionaryUrl = "http://manga-panda.xyz/popular-manga/?page=";
}

bool MangaPanda::updateMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<a href="(http://manga-panda.xyz/manga/[^"]*?)" title="([^"]*?)">)lit");

    QRegularExpression numpagesrx(R"lit(>(\d+)</a></li>\W*<li><a[^>]+rel="next">&raquo;</a></li>)lit");

    auto job = networkManager->downloadAsString(dictionaryUrl + "1");

    if (!job->await(10000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto numpagesrxmatch = numpagesrx.match(job->bufferStr);

    MangaList mangas;
    mangas.absoluteUrls = true;
    int pages = 1;

    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();
    qDebug() << "pages:" << pages;

    const int matchesPerPage = 30;
    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        int spos = job->bufferStr.indexOf(R"(<span>Popular Manga</span>)");
        int epos = job->bufferStr.indexOf(R"(<li class="active">)");

        for (auto &match : getAllRxMatches(mangarx, job->bufferStr, spos, epos))
        {
            auto title = htmlToPlainText(match.captured(2));
            auto url = match.captured(1);
            mangas.append(title, url);
            matches++;
        }

        token->sendProgress(10 + 90 * (mangas.size / matchesPerPage) / pages);
        qDebug() << "matches:" << matches;
        if (matches < matchesPerPage)
            qDebug() << "       Incomplete match in page:" << job->url;
    };

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i < pages; i++)
        urls.append(dictionaryUrl + QString::number(i));

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

Result<MangaChapterCollection, QString> MangaPanda::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"(<li>Author\(s\) ?:([^<]*))");
    QRegularExpression artistrx;
    QRegularExpression statusrx("<li>Status ?:([^<]*)");
    QRegularExpression yearrx;
    QRegularExpression genresrx("<li>Genre ?:(.*?)</li>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(R"(<div id="noidungm"[^>]*>(.*?)</div>)",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"(<div class="manga-info-pic">\W*<img src="([^"]*))");

    QRegularExpression chapterrx(R"lit(<span><a\W*href="([^"]*)"\W*title="([^"]*)">)lit");

    fillMangaInfo(info, job->bufferStr, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    info->author = info->author.remove(',');

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->bufferStr))
    {
        auto ctitle = chapterrxmatch.captured(2);
        auto curl = chapterrxmatch.captured(1);
        newchapters.prepend(MangaChapter(ctitle, curl));
    }

    return Ok(newchapters);
}

Result<QStringList, QString> MangaPanda::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit(<p id=arraydata style=display:none>(.*?)</p>)lit");
    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    auto pagerxmatch = pagerx.match(job->bufferStr);

    if (!pagerxmatch.hasMatch())
        return Err(QString("Couldn't parse pages."));

    QStringList imageUrls;
    for (auto &match : pagerxmatch.captured(1).split(','))
    {
        imageUrls.append(match);
    }
    return Ok(imageUrls);
}
