#include "mangapanda.h"

MangaPanda::MangaPanda(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaPanda";
    baseUrl = "http://www.mangapanda.com";
    dictionaryUrl = baseUrl + "/popular/";
}

bool MangaPanda::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<a href="(/[^"]+)">([^<]+)<)lit");

    QRegularExpression numpagesrx(R"lit(href="/popular/(\d+)">Last)lit");

    auto job = networkManager->downloadAsString(dictionaryUrl);

    if (!job->await(10000))
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
        pages = numpagesrxmatch.captured(1).toInt() / 30 + 1;
    qDebug() << "pages:" << pages;

    const int matchesPerPage = 30;
    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int spos = job->buffer.indexOf(R"(>Popular Mangas)");
        int epos = job->buffer.indexOf(R"(<li class="active">)");

        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer, spos, epos))
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
    for (int i = 1; i < pages; i++)
        urls.append(dictionaryUrl + QString::number(i * 30));

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

void MangaPanda::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                                QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx("Author ?:</td>[^>]*>([^<]*)");
    QRegularExpression artistrx("Artist ?:</td>[^>]*>([^<]*)");
    QRegularExpression statusrx("Status ?:</td>[^>]*>([^<]*)");
    QRegularExpression yearrx("Year of Release ?:</td>[^>]*>([^<]*)");
    QRegularExpression genresrx("Genre ?:</td>(.*?)</td>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(R"(>Read [^<]* Online</div><p>(.*?)</p>)",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"(<img src="([^"]*/cover/[^"]*))");

    QRegularExpression chapterrx(R"lit(<a href="([^"]*)"[^>]*>([^<]*)</a>([^<]*))lit");

    fillMangaInfo(info, job->buffer, authorrx, artistrx, statusrx, yearrx, QRegularExpression(), summaryrx,
                  coverrx);

    int spos = job->buffer.indexOf(R"(Date Added)");
    int epos = job->buffer.indexOf(R"(<script>)", spos);

    auto genresrxmatch = genresrx.match(job->buffer);
    info->genres = htmlToPlainText(genresrxmatch.captured(1).replace("</a>", " ")).trimmed();

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
    {
        auto ctitle = chapterrxmatch.captured(2);
        if (chapterrxmatch.captured(3) != " : ")
            ctitle += chapterrxmatch.captured(3);
        auto curl = baseUrl + chapterrxmatch.captured(1);
        newchapters.append(MangaChapter(ctitle, curl));
    }
    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> MangaPanda::getPageList(const QString &chapterUrl)
{
    QRegularExpression pagerx(R"lit("u":"([^"]*)")lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    QStringList imageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->buffer))
    {
        imageUrls.append(match.captured(1).remove('\\'));
    }
    return Ok(imageUrls);
}
