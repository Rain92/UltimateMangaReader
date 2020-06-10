#include "mangatown.h"

MangaTown::MangaTown(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "MangaTown";
    baseurl = "https://www.mangatown.com";
    dictionaryUrl = "https://www.mangatown.com/directory/";
}

bool MangaTown::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression mangarx(R"lit(<a class="manga_cover" href="(/manga/[^"]*?)" title="([^"]*?)")lit");

    QRegularExpression numpagesrx(R"(\.\.\.<a href="/directory/(\d{3,4}).htm")");

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
    qDebug() << "pages:" << pages;

    const int matchesPerPage = 30;
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
            qDebug() << "          Incomplete match in page:" << job->url;
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

void MangaTown::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                               QSharedPointer<MangaInfo> info)
{
    //    QRegularExpression titlerx(R"(<h1 class="title-top">([^<]*?)<)");
    QRegularExpression authorrx(R"(<b>Author\(s\):</b>(.*?)<li>)");
    QRegularExpression artistrx(R"(<b>Artist\(s\):</b>(.*?)<li>)");
    QRegularExpression statusrx(R"(<b>Status\(s\):</b>(.*?)(?:<a|<li))");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(<b>Genre\(s\):</b>(.*?)</li>)");

    QRegularExpression summaryrx(R"lit(<span id="show"[^>]*?>([^<]*?)<)lit");

    QRegularExpression coverrx(R"lit(<img src="([^"]*?)" onerror="this.src)lit");

    QRegularExpression chapterrx(R"lit(<a href="(/manga/[^"]*?)"[^>]*?>([^<]*))lit");

    fillMangaInfo(info, job->buffer, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    int spos = job->buffer.indexOf(R"(<ul class="chapter_list">)");
    int epos = job->buffer.indexOf(R"(<div class="comment_content">)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer, spos, epos))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), baseurl + chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> MangaTown::getPageList(const QString &chapterUrl)
{
    QRegularExpression numPagesRx(
        R"lit(>(\d+)</option>\s*?(:?<option value="/manga/[^"]*?">Featured</option>)?\s*?</select>)lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    auto numPagesRxMatch = numPagesRx.match(job->buffer);

    if (!numPagesRxMatch.hasMatch())
        return Err(QString("Couldn't process pagelist."));

    int numPages = numPagesRxMatch.captured(1).toInt();

    QStringList imageUrls;
    for (int i = 1; i <= numPages; i++)
    {
        auto url = QString("%1%2.html").arg(chapterUrl).arg(i);
        imageUrls.append(url);
    }

    return Ok(imageUrls);
}
Result<QString, QString> MangaTown::getImageUrl(const QString &pageUrl)
{
    QRegularExpression imgUrlRx(R"lit(<img\s*(?:id="image")?\s*src="([^"]*?)"\s*(?:id="image")?)lit");

    auto job = networkManager->downloadAsString(pageUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    auto match = imgUrlRx.match(job->buffer);

    if (!match.hasMatch())
        return Err(QString("Couldn't process pages/images."));

    auto imageUrl = match.captured(1);
    if (imageUrl.startsWith("//"))
        imageUrl.prepend("https:");

    return Ok(imageUrl);
}
