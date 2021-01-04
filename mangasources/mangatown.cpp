#include "mangatown.h"

MangaTown::MangaTown(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "MangaTown";
    baseUrl = "https://www.mangatown.com";
    dictionaryUrl = "https://www.mangatown.com/directory/";
}

bool MangaTown::updateMangaList(UpdateProgressToken *token)
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

    auto numpagesrxmatch = numpagesrx.match(job->bufferStr);

    MangaList mangas;
    mangas.absoluteUrls = false;

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();
    qDebug() << "pages:" << pages;

    const int matchesPerPage = 30;
    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->bufferStr))
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

Result<MangaChapterCollection, QString> MangaTown::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"(<b>Author\(s\):</b>(.*?)<li>)");
    QRegularExpression artistrx(R"(<b>Artist\(s\):</b>(.*?)<li>)");
    QRegularExpression statusrx(R"(<b>Status\(s\):</b>(.*?)(?:<a|<li))");
    QRegularExpression yearrx;
    QRegularExpression genresrx(R"(<b>Genre\(s\):</b>(.*?)</li>)");

    QRegularExpression summaryrx(R"lit(<span id="show"[^>]*?>([^<]*?)<)lit");

    QRegularExpression coverrx(R"lit(<img src="([^"]*?)" onerror="this.src)lit");

    QRegularExpression chapterrx(R"lit(<a href="(/manga/[^"]*?)"[^>]*?>([^<]*))lit");

    fillMangaInfo(info, job->bufferStr, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx, coverrx);

    int spos = job->bufferStr.indexOf(R"(<ul class="chapter_list">)");
    int epos = job->bufferStr.indexOf(R"(<div class="comment_content">)", spos);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->bufferStr, spos, epos))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2), baseUrl + chapterrxmatch.captured(1)));

    return Ok(newchapters);
}

Result<QStringList, QString> MangaTown::getPageList(const QString &chapterUrl)
{
    QRegularExpression numPagesRx(
        R"lit(>(\d+)</option>\s*?(:?<option value="/manga/[^"]*?">Featured</option>)?\s*?</select>)lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(7000))
        return Err(job->errorString);

    auto numPagesRxMatch = numPagesRx.match(job->bufferStr);

    if (!numPagesRxMatch.hasMatch())
        return Err(QString("Couldn't process pagelist."));

    int numPages = numPagesRxMatch.captured(1).toInt();

    QStringList imageUrls;
    for (int i = 1; i <= numPages; i++)
    {
        auto url = QString("%1%2.html").arg(job->url).arg(i);
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

    auto match = imgUrlRx.match(job->bufferStr);

    if (!match.hasMatch())
        return Err(QString("Couldn't process pages/images."));

    auto imageUrl = match.captured(1);
    if (imageUrl.startsWith("//"))
        imageUrl.prepend("https:");

    return Ok(imageUrl);
}
