#include "mangago.h"

MangaGo::MangaGo(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaGo";
    baseUrl = "http://www.mangago.me/";
    mangalistUrl = "http://www.mangago.me/list/directory/all/";
}

bool MangaGo::updateMangaList(UpdateProgressToken *token)
{
    QRegularExpression numpagesrx(R"lit(class="pagination"[^>]*total="(\d+)")lit");

    QRegularExpression mangarx(R"lit(href='([^']*)'[^>]*alt='[^']*'[^>]*title='([^']*)')lit");

    QRegularExpression nummangasrx("Total: ([0-9,]+)");

    auto job = networkManager->downloadAsString(mangalistUrl + "1");

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto numpagesrxmatch = numpagesrx.match(job->buffer);
    auto nummangasrxmatch = nummangasrx.match(job->buffer);

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    int nominalSize = 0;
    if (nummangasrxmatch.hasMatch())
        nominalSize = nummangasrxmatch.captured(1).remove(',').toInt();

    MangaList mangas;
    mangas.absoluteUrls = true;

    qDebug() << "pages:" << pages;

    const int matchesPerPage = 44;
    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer))
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

    if (nominalSize != mangas.size)
        qDebug() << "Not all mangas captured:" << nominalSize << "vs" << mangas.size;

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(mangalistUrl + QString::number(i));

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

void MangaGo::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                             QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"(<label>\W*Author:\W*</label>(.*?)\d* released.\W*</td>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression artistrx;
    QRegularExpression statusrx(R"(<label>\W*Status:\W*</label>(.*?)</)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression yearrx(R"(<label>\W*Author:\W*</label>.*?(\d*) released.\W*</td>)",
                              QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression genresrx(R"(<label>\W*Genre\(s\):\W*</label>(.*?)</td>)",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression summaryrx(R"lit(<div class="manga_summary">(.*?)</div>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression coverrx(R"lit(<meta property="og:image" content="([^"]*)")lit");

    QRegularExpression chapterrx(R"lit(<a[^>]*href="([^"]*)">W*(.*?)W*</a>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);

    fillMangaInfo(info, job->buffer, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    int spos = job->buffer.indexOf(R"(id="chapter_table">)");
    int epos = job->buffer.indexOf(R"(</table>)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
        newchapters.insert(
            0, MangaChapter(htmlToPlainText(chapterrxmatch.captured(2)), chapterrxmatch.captured(1)));

    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> MangaGo::getPageList(const QString &chapterUrl)
{
    // TODO
    QRegularExpression pagerx(R"lit()lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    QStringList imageUrls;
    for (auto &match : getAllRxMatches(pagerx, job->buffer))
    {
        imageUrls.append(match.captured(1));
    }

    return Ok(imageUrls);
}
