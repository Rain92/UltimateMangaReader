#include "readmanga.h"

ReadManga::ReadManga(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "ReadManga";
    baseUrl = "https://www.readm.org";
    dictionaryUrl = "https://readm.org/manga-list/";
}

bool ReadManga::updateMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<a href="(\/manga\/[^"]*?)".*?<h2[^>]*>([^<]*)<)lit",
                               QRegularExpression::DotMatchesEverythingOption);

    QElapsedTimer timer;
    timer.start();

    QList<QString> urls;
    for (char letter = 'a'; letter <= 'z'; letter++)
        urls.append(dictionaryUrl + letter);

    urls.append(dictionaryUrl);

    MangaList mangas;
    mangas.absoluteUrls = false;

    std::atomic_int counter = 0;

    auto lambda = [&](QSharedPointer<DownloadStringJob> job)
    {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->bufferStr))
        {
            auto title = htmlToPlainText(match.captured(2));
            auto url = match.captured(1);
            mangas.append(title, url);
            matches++;
        }

        counter++;

        token->sendProgress(counter * 100 / urls.count());
        qDebug() << "matches:" << matches;
    };

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

Result<MangaChapterCollection, QString> ReadManga::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"lit(Author[^<]*<a[^>]*>[^<]*<small>([^<]*)<)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression artistrx(R"lit(Artist[^<]*<a[^>]*>[^<]*<small>([^<]*)<)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression statusrx;
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(Genres:(.*?)</div>)", QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression summaryrx(R"lit(SUMMARY</h2>(.*?)</p>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression coverrx(R"lit(<img class="series-profile-thumb"[^>]*?src="([^"]*?)")lit");

    QRegularExpression chapterrx(
        R"lit(class="table-episodes-title">.*?<a href="(\/manga\/[^"]*?)"[^>]*?>([^<]*?)</a>)lit",
        QRegularExpression::DotMatchesEverythingOption);

    fillMangaInfo(info, job->bufferStr, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    info->coverUrl = baseUrl + info->coverUrl;

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->bufferStr))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), baseUrl + chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);

    return Ok(newchapters);
}

Result<QStringList, QString> ReadManga::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit(,"src":"([^"]*?)")lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    int spos = job->bufferStr.indexOf("chapter['pages']");
    int epos = job->bufferStr.indexOf('\n', spos);

    QStringList imageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->bufferStr, spos, epos))
    {
        auto imageUrl = match.captured(1).remove('\\');
        imageUrls.append(baseUrl + imageUrl);
    }

    return Ok(imageUrls);
}
