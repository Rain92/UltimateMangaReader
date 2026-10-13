#include "mangahere.h"

// reads a single quoted javascript string literal, pos points to the opening quote
static QString readJsString(const QString &str, int &pos)
{
    QString result;
    if (pos >= str.length() || str[pos] != '\'')
        return result;

    for (pos++; pos < str.length(); pos++)
    {
        auto ch = str[pos];
        if (ch == '\\' && pos + 1 < str.length())
        {
            result.append(str[++pos]);
        }
        else if (ch == '\'')
        {
            pos++;
            break;
        }
        else
        {
            result.append(ch);
        }
    }

    return result;
}

// unpacks scripts packed with eval(function(p,a,c,k,e,d){...}('payload',radix,count,'k|e|y|s'.split('|'),0,{}))
static QString unpackJs(const QString &packed)
{
    static const QString digits = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QRegularExpression argsrx(R"(^,\s*(\d+)\s*,\s*(\d+)\s*,\s*)");
    QRegularExpression wordrx(R"(\b\w+\b)");

    int pos = packed.indexOf("}('");
    if (pos < 0)
        return "";
    pos += 2;

    auto payload = readJsString(packed, pos);

    auto argsmatch = argsrx.match(packed.mid(pos, 40));
    if (!argsmatch.hasMatch())
        return "";

    int radix = argsmatch.captured(1).toInt();
    int count = argsmatch.captured(2).toInt();
    pos += argsmatch.capturedEnd();

    auto keywords = readJsString(packed, pos).split('|');

    QString result;
    int last = 0;
    auto it = wordrx.globalMatch(payload);
    while (it.hasNext())
    {
        auto match = it.next();
        auto word = match.captured(0);

        int value = 0;
        bool valid = radix > 0 && (word.length() == 1 || word[0] != '0');
        for (int i = 0; valid && i < word.length(); i++)
        {
            int d = digits.indexOf(word[i]);
            valid = d >= 0 && d < radix;
            value = value * radix + d;
            valid = valid && value < count;
        }

        result.append(payload.mid(last, match.capturedStart() - last));
        if (valid && value < keywords.count() && !keywords[value].isEmpty())
            result.append(keywords[value]);
        else
            result.append(word);
        last = match.capturedEnd();
    }
    result.append(payload.mid(last));

    return result;
}

static QString normalizeUrl(QString url)
{
    url.replace("&amp;", "&");
    if (url.startsWith("//"))
        url.prepend("https:");

    return url;
}

MangaHere::MangaHere(NetworkManager *networkManager) : AbstractMangaSource(networkManager)
{
    name = "MangaHere";
    baseUrl = "https://www.mangahere.cc";
    mobileUrl = "https://m.mangahere.cc";
    fanfoxUrl = "https://fanfox.net";
    dictionaryUrl = "https://www.mangahere.cc/directory/";
    mangaListUrl = "https://www.mangahere.cc/mangalist/";

    // the info page of long series is ~600 KB (~40 KB gzipped)
    mangaInfoTimeout = 5000;

    networkManager->addCookie(".mangahere.cc", "isAdult", "1");
    networkManager->addCookie(".fanfox.net", "isAdult", "1");

    // images and covers are hotlink protected, chapterfun.ashx answers empty without referer
    networkManager->addSetCustomRequestHeader("mangahere.cc", "Referer", R"(https://www.mangahere.cc/)");
    networkManager->addSetCustomRequestHeader("mangahere.org", "Referer", R"(https://www.mangahere.cc/)");
    networkManager->addSetCustomRequestHeader("mangahere.com", "Referer", R"(https://www.mangahere.cc/)");
    networkManager->addSetCustomRequestHeader("fanfox.net", "Referer", R"(https://fanfox.net/)");
    networkManager->addSetCustomRequestHeader("mangafox.me", "Referer", R"(https://fanfox.net/)");
}

bool MangaHere::updateMangaList(UpdateProgressToken *token)
{
    QRegularExpression popularrx(
        R"lit(<a href="(/manga/[^"]*)" title="([^"]*)"><img class="manga-list-1-cover")lit");
    QRegularExpression mangarx(
        R"lit(<p class="browse-new-block-content"><a href="(/manga/[^"]*)" title="([^"]*)">)lit");

    // the directory has 560+ pages, /mangalist/ has the whole catalogue on one page (~1 MB gzipped)
    // only the first directory pages are loaded for the popularity order
    const int popularPages = 10;

    QElapsedTimer timer;
    timer.start();

    // a retry only loads the list itself again
    auto &state = token->resumeState<MangaHereUpdateState>();

    token->sendProgress(state.popularDone ? 10 : 0);

    auto job = networkManager->downloadAsString(mangaListUrl, 180000);

    if (!state.popularDone)
    {
        QList<QString> urls;
        for (int i = 1; i <= popularPages; i++)
            urls.append(dictionaryUrl + QString::number(i) + ".htm");

        state.popular.fill({}, popularPages);
        auto lambda = [&](QSharedPointer<DownloadStringJob> pjob)
        {
            int i = urls.indexOf(pjob->originalUrl);
            if (i < 0)
                return;

            for (auto &match : getAllRxMatches(popularrx, pjob->bufferStr))
                state.popular[i].append({match.captured(2), match.captured(1)});
        };

        // failed pages only degrade the popularity order
        DownloadQueue queue(networkManager, urls, CONF.parallelDownloadsLow, lambda, false);
        queue.setCancellationToken(&token->canceled);
        queue.start();
        queue.awaitCompletion();

        state.popularDone = !token->canceled;
    }

    token->sendProgress(10);

    for (int attempt = 1;; attempt++)
    {
        while (!job->isCompleted && job->errorString.isEmpty() && !token->canceled)
            awaitSignal(job.get(), 500, &DownloadJobBase::completed, &DownloadJobBase::downloadError);

        if (token->canceled)
        {
            token->sendError("Download cancelled");
            return false;
        }

        // the list is occasionally cut off
        if (job->isCompleted && job->bufferStr.contains(R"(<div class="footer-bar">)"))
            break;

        if (attempt >= 2)
        {
            token->sendError(job->isCompleted ? QString("Manga list incomplete.") : job->errorString);
            return false;
        }

        qDebug() << "MangaHere: manga list incomplete, retrying" << job->errorString;
        job = networkManager->downloadAsString(mangaListUrl, 180000);
    }

    token->sendProgress(80);

    MangaList mangas;
    mangas.absoluteUrls = false;

    QSet<QString> mangaUrls;
    auto addManga = [&](const QString &rawTitle, const QString &url)
    {
        if (mangaUrls.contains(url))
            return;

        auto title = rawTitle.contains('&') ? htmlToPlainText(rawTitle) : rawTitle;
        mangaUrls.insert(url);
        mangas.append(title.trimmed(), url);
    };

    for (const auto &page : std::as_const(state.popular))
        for (const auto &entry : page)
            addManga(entry.first, entry.second);
    int popularCount = mangas.size;

    int matches = 0;
    auto it = mangarx.globalMatch(job->bufferStr);
    while (it.hasNext())
    {
        auto match = it.next();
        addManga(match.captured(2), match.captured(1));
        matches++;
    }

    qDebug() << "popular:" << popularCount << "matches:" << matches;

    if (matches == 0)
    {
        token->sendError("Couldn't process manga list.");
        return false;
    }

    this->mangaList = mangas;

    qDebug() << "mangas:" << mangas.size << "time:" << timer.elapsed();

    token->sendProgress(100);

    return true;
}

Result<MangaChapterCollection, QString> MangaHere::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression authorrx(R"lit(<p class="detail-info-right-say">\s*Author:(.*?)</p>)lit");
    QRegularExpression authornamerx(R"lit(<a[^>]*>([^<]*)</a>)lit");
    QRegularExpression statusrx(R"lit(<span class="detail-info-right-title-tip">([^<]*)<)lit");
    QRegularExpression genresrx(R"lit(<p class="detail-info-right-tag-list">(.*?)</p>)lit",
                                QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression summaryrx(R"lit(<p[^>]*class="fullcontent">(.*?)</p>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression coverrx(R"lit(<img class="detail-info-cover-img" src="([^"]*)")lit");

    QRegularExpression blockedrx(R"lit(<p class="detail-block-content">(.*?)</p>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpression chapterrx(
        R"lit(<a href="(/manga/[^"]*)"[^>]*>\s*<div class="detail-main-list-main">\s*<p class="title3">([^<]*)</p>)lit");

    const QString &buffer = job->bufferStr;

    if (!buffer.contains(R"(class="detail-info")"))
        return Err(QString("Couldn't process manga info."));

    // don't drop chapters because of a cut off page
    if (!buffer.contains(R"(<div class="footer-bar">)"))
        return Err(QString("Manga info incomplete."));

    // blocked titles and titles without chapters show a message instead of the chapter list
    int spos = buffer.indexOf(R"(<div id="chapterlist">)");
    if (spos < 0)
    {
        auto blockedmatch = blockedrx.match(buffer);
        return Err(blockedmatch.hasMatch() ? htmlToPlainText(blockedmatch.captured(1)).trimmed()
                                           : QString("Couldn't process chapters."));
    }

    int epos = buffer.indexOf(R"(class="detail-main-list-more")", spos);
    auto matches = getAllRxMatches(chapterrx, buffer, spos, epos);
    if (matches.isEmpty())
        return Err(QString("Couldn't process chapters."));

    // summary and author are parsed here, fillMangaInfo would strip "[...]" from them
    fillMangaInfo(info, buffer, QRegularExpression(), QRegularExpression(), statusrx, QRegularExpression(),
                  genresrx, QRegularExpression(), coverrx);

    // placeholder links like "[Add]" aren't authors
    QStringList authors;
    for (auto &match : getAllRxMatches(authornamerx, authorrx.match(buffer).captured(1)))
    {
        auto name = htmlToPlainText(match.captured(1)).trimmed();
        if (!name.isEmpty() && !(name.startsWith('[') && name.endsWith(']')))
            authors.append(name);
    }
    info->author = authors.join(", ");

    // keep the paragraphs
    auto summary = summaryrx.match(buffer).captured(1).trimmed();
    info->summary = htmlToPlainText(summary.remove('\r').replace('\n', "<br>")).trimmed();

    info->status = info->status.trimmed();
    info->coverUrl = normalizeUrl(info->coverUrl);

    // newest chapter first on the website
    MangaChapterCollection newchapters;
    QSet<QString> chapterUrls;
    for (int i = matches.count() - 1; i >= 0; i--)
    {
        auto url = baseUrl + matches[i].captured(1);
        if (chapterUrls.contains(url))
            continue;
        chapterUrls.insert(url);

        auto title = matches[i].captured(2).trimmed();
        if (title.contains('&'))
            title = htmlToPlainText(title).trimmed();

        newchapters.append(MangaChapter(title, url));
    }

    return Ok(newchapters);
}

Result<QStringList, QString> MangaHere::getPageList(const QString &chapterUrl)
{
    QRegularExpression chapterpathrx(R"lit(/manga/(.+/)[^/]*$)lit");
    QRegularExpression imagerx(R"lit(<img class="lazy" data-original="([^"]*)")lit");
    QRegularExpression numpagesrx(R"lit(<div class="roll-page">\d+/(\d+)</div>)lit");

    // both readers together shouldn't block much longer than a single request
    QElapsedTimer timer;
    timer.start();

    // the mobile long strip reader has all images on one page, but not for licensed and adult titles
    auto pathmatch = chapterpathrx.match(chapterUrl);
    if (pathmatch.hasMatch())
    {
        auto job = networkManager->downloadAsString(mobileUrl + "/roll_manga/" + pathmatch.captured(1));

        if (job->await(4000, false))
        {
            QStringList imageUrls;
            for (auto &match : getAllRxMatches(imagerx, job->bufferStr))
                imageUrls.append(normalizeUrl(match.captured(1)));

            auto numpagesmatch = numpagesrx.match(job->bufferStr);
            if (!imageUrls.isEmpty() && numpagesmatch.hasMatch() &&
                numpagesmatch.captured(1).toInt() == imageUrls.count())
                return Ok(imageUrls);
        }
        qDebug() << "MangaHere: mobile reader unavailable:" << job->errorString;
    }

    return getPageListDesktop(chapterUrl, qMax(4000, 8000 - int(timer.elapsed())));
}

Result<QStringList, QString> MangaHere::getPageListDesktop(const QString &chapterUrl, int timeout)
{
    QRegularExpression chapteridrx(R"(var chapterid\s*=\s*(\d+))");
    QRegularExpression imagecountrx(R"(var imagecount\s*=\s*(\d+))");
    QRegularExpression blockedrx(R"lit(<p class="detail-block-content">(.*?)</p>)lit",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression packedrx(R"lit(eval\(function\(p,a,c,k,e,d\).*?</script>)lit",
                                QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression quotedrx(R"lit('([^']*)')lit");
    QRegularExpression imageinfosrx(R"lit(newImginfos=\[([^\]]*)\])lit");

    auto job = networkManager->downloadAsString(chapterUrl);

    if (!job->await(timeout))
        return Err(job->errorString);

    // removed or blocked chapters, the sister site fanfox.net often still has the blocked ones
    auto blockedmatch = blockedrx.match(job->bufferStr);
    if (blockedmatch.hasMatch())
    {
        if (chapterUrl.startsWith(baseUrl))
        {
            auto fanfoxR = getPageListDesktop(fanfoxUrl + chapterUrl.mid(baseUrl.length()), 4000);
            if (fanfoxR.isOk())
                return fanfoxR;
        }
        return Err(htmlToPlainText(blockedmatch.captured(1)).trimmed());
    }

    QString key;
    for (auto &match : getAllRxMatches(packedrx, job->bufferStr))
    {
        auto script = unpackJs(match.captured(0));

        if (script.contains("newImgs="))
        {
            // long strip reader: all images are in the page
            int spos = script.indexOf("newImgs=");
            int epos = script.indexOf("];", spos);

            QStringList imageUrls;
            for (auto &imgmatch : getAllRxMatches(quotedrx, script, spos, epos))
                imageUrls.append(normalizeUrl(imgmatch.captured(1)));

            // the last image is an advertisement that reuses the id of its predecessor
            auto infos = imageinfosrx.match(script).captured(1).split(',');
            if (imageUrls.count() > 1 && infos.count() == imageUrls.count() &&
                infos.last() == infos[infos.count() - 2])
                imageUrls.removeLast();

            if (!imageUrls.isEmpty())
                return Ok(imageUrls);
        }
        else if (script.contains("guidkey"))
        {
            for (auto &keymatch : getAllRxMatches(quotedrx, script, 0, script.indexOf(';')))
                key += keymatch.captured(1);
        }
    }

    auto chapteridmatch = chapteridrx.match(job->bufferStr);
    auto imagecountmatch = imagecountrx.match(job->bufferStr);

    if (!chapteridmatch.hasMatch() || !imagecountmatch.hasMatch())
        return Err(QString("Couldn't process pagelist."));

    // the last page is always an advertisement
    int numPages = imagecountmatch.captured(1).toInt();
    if (numPages > 1)
        numPages--;

    auto funUrl = job->url.left(job->url.lastIndexOf('/') + 1) +
                  "chapterfun.ashx?cid=" + chapteridmatch.captured(1) + "&page=";

    QStringList pageUrls;
    for (int i = 1; i <= numPages; i++)
        pageUrls.append(funUrl + QString::number(i) + "&key=" + key);

    return Ok(pageUrls);
}

Result<QString, QString> MangaHere::getImageUrl(const QString &pageUrl)
{
    if (!pageUrl.contains("/chapterfun.ashx"))
        return Ok(pageUrl);

    QRegularExpression pixrx(R"lit(var pix="([^"]*)")lit");
    QRegularExpression pvaluerx(R"lit(var pvalue=\["([^"]*)")lit");

    QElapsedTimer timer;
    timer.start();

    auto url = pageUrl;
    for (int attempt = 0; attempt < 3 && timer.elapsed() < 5000; attempt++)
    {
        auto job = networkManager->downloadAsString(url);

        if (!job->await(6000 - int(timer.elapsed())))
            return Err(job->errorString);

        // the answer is sometimes empty, retry without key
        if (job->bufferStr.isEmpty())
        {
            url = url.left(url.indexOf("&key=")) + "&key=";
            continue;
        }

        auto script = unpackJs(job->bufferStr);
        auto pixmatch = pixrx.match(script);
        auto pvaluematch = pvaluerx.match(script);

        if (!pixmatch.hasMatch() || !pvaluematch.hasMatch())
            break;

        return Ok(normalizeUrl(pixmatch.captured(1) + pvaluematch.captured(1)));
    }

    return Err(QString("Couldn't process pages/images."));
}
