#include "mangakakalot.h"

Mangakakalot::Mangakakalot(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "Mangakakalot";
    baseUrl = "https://manganato.com/";
    dictionaryUrl = baseUrl + "genre-all/%1?type=topview";

    networkManager->addCookie("mangakakalot.com", "content_lazyload", "off");
    networkManager->addCookie("manganelo.com", "content_lazyload", "off");
    networkManager->addCookie("manganato.com", "content_lazyload", "off");

    networkManager->addSetCustomRequestHeader(".mkkl", "Referer", R"(https://readmanganato.com/)");
}

bool Mangakakalot::updateMangaList(UpdateProgressToken *token)
{
    QString rxstart(R"(panel-content-genres)");
    QString rxend(R"(panel-genres-list)");
    QRegularExpression mangarx(
        R"lit(class="content-genres-item">\s*<a[^>]* href="([^"]*)"\s*title="([^"]*)")lit");

    QRegularExpression nummangasrx(R"((?:Total\s?:|TOTAL\s?:)\s?([0-9,]+))");
    QRegularExpression numpagesrx(R"(>(?:LAST|Last)\s?\(([0-9,]+)\)<)");

    auto job = networkManager->downloadAsString(dictionaryUrl.arg(1));

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto nummangasrxmatch = nummangasrx.match(job->bufferStr);
    auto numpagesrxmatch = numpagesrx.match(job->bufferStr);

    int nominalSize = 0;
    if (nummangasrxmatch.hasMatch())
        nominalSize = nummangasrxmatch.captured(1).remove(',').toInt();

    MangaList mangas;
    mangas.absoluteUrls = true;

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    const int matchesPerPage = 24;
    auto lambda = [&](QSharedPointer<DownloadStringJob> job)
    {
        int spos = job->bufferStr.indexOf(rxstart);
        int epos = job->bufferStr.indexOf(rxend);

        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->bufferStr, spos, epos))
        {
            auto title = htmlToPlainText(match.captured(2));
            auto url = match.captured(1);
            mangas.append(title, url);
            matches++;
        }

        token->sendProgress(10 + 90 * (mangas.size / matchesPerPage) / pages);
        qDebug() << "matches:" << matches;
    };

    if (nominalSize != mangas.size)
        qDebug() << "Not all mangas captured:" << nominalSize << "vs" << mangas.size;

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(dictionaryUrl.arg(i));

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
Result<MangaChapterCollection, QString> Mangakakalot::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"(author/[^>]*?>([^<]*?)<)");
    QRegularExpression artistrx;
    QRegularExpression statusrx(R"(Status :(?:\s|</td>\s)(.*?)(?:</li>|</td>))",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(Genres :[^>]*>(.*?)(?:</li>|</tr>))",
                                QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression summaryrx(R"lit((?:Description :</h3>|summary: </p></h2>)(.*?)</div)lit",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression coverrx(R"lit(<meta name="twitter:image" content="([^"]*)")lit");

    QRegularExpression chapterrx(R"lit(<a[^>]*?href="([^"]*)"[^>]*>([^<]*)<)lit");

    fillMangaInfo(info, job->bufferStr, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    info->genres = info->genres.remove("- ");
    info->status = info->status.remove('\n');
    if (info->status.contains('-'))
        info->status = info->status.split('-')[0];

    int spos = job->bufferStr.indexOf(R"(chapter-list">)");
    int epos = job->bufferStr.indexOf(R"(<div class="fb-comments)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->bufferStr, spos, epos))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), chapterrxmatch.captured(1)));

    return Ok(newchapters);
}

Result<QStringList, QString> Mangakakalot::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit(<img src="([^"]*)")lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    int spos = job->bufferStr.indexOf(R"(<div class="vung-doc" id="vungdoc">)");
    if (spos < 0)
        spos = job->bufferStr.indexOf(R"(<div class="container-chapter-reader">)");
    int epos = job->bufferStr.indexOf(R"(class="navi-change-chapter">)", spos);

    QStringList imageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->bufferStr, spos, epos))
    {
        auto imageUrl = match.captured(1);
        if (!imageUrl.contains("/themes/"))
            imageUrls.append(imageUrl);
    }

    return Ok(imageUrls);
}
