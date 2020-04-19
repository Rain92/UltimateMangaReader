#include "mangadex.h"

MangaDex::MangaDex(NetworkManager *dm) : AbstractMangaSource(dm)
{
    name = "MangaDex";
    baseurl = "https://mangadex.org";

    //    networkManager->addCookie(".mangadex.org", "mangadex_h_toggle", "1");
    networkManager->addCookie(".mangadex.org", "mangadex_title_mode", "2");
    networkManager->addCookie(".mangadex.org", "mangadex_filter_langs", "1");

    //    login();
}

void MangaDex::login()
{
    QUrlQuery postData;
    postData.addQueryItem("login_username", "UMRBot2");
    postData.addQueryItem("login_password", "umrbot123");
    postData.addQueryItem("remember_me", "1");
    auto query = postData.query().toUtf8();

    QString loginurl("https://mangadex.org/ajax/actions.ajax.php?function=login&nojs=1");

    auto lambda = [this](QSharedPointer<DownloadJobBase> job) {
        auto ncookies = job->getCookies();
        foreach (QNetworkCookie c, ncookies)
        {
            qDebug() << "Added cookie" << c.name() << c.value();
            networkManager->addCookie(".mangadex.org", c.name(), c.value());
        }
    };

    auto job = networkManager->downloadAsString(loginurl, 6000, query);

    executeOnJobCompletion(job, lambda);
}

bool MangaDex::uptareMangaList(UpdateProgressToken *token)
{
    QRegularExpression nummangasrx(R"(<p class=[^>]*>Showing .*? (\d+,\d+) titles)");

    QRegularExpression mangarx(
        R"lit(<a title=['"]([^'"]*?)['"][^<]*?href=['"]([^'"]*?)['"][^<]*?class=")lit");

    MangaList mangas;

    QString basedictlink = baseurl + "/titles/2/";

    auto job = networkManager->downloadAsString(basedictlink + "1", -1);

    if (!job->await(7000))
    {
        token->sendError(job->errorString);
        return false;
    }

    token->sendProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto nummangasrxmatch = nummangasrx.match(job->buffer);

    int nominalSize = 0;
    if (nummangasrxmatch.hasMatch())
        nominalSize = nummangasrxmatch.captured(1).remove(',').toInt();

    int pages = (nominalSize + 99) / 100;
    //    pages = 5;
    qDebug() << "pages" << pages;

    auto lambda = [&](QSharedPointer<DownloadJobBase> job) {
        auto sjob = static_cast<DownloadStringJob *>(job.get());
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, sjob->buffer))
        {
            mangas.links.append(match.captured(2) + "/chapters/");
            mangas.titles.append(htmlToPlainText(htmlToPlainText(match.captured(1))));
            matches++;
        }
        mangas.size += matches;

        token->sendProgress(10 + 90 * mangas.size / nominalSize);

        qDebug() << "matches:" << matches;
    };

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(basedictlink + QString::number(i));

    DownloadQueue queue(networkManager, urls, 2, lambda, true);
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

void MangaDex::updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                              QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(class="mx-1">([^<]*)<)");

    QRegularExpression authorrx("Author:</div>[^>]*>[^>]*>([^<]*)");
    QRegularExpression artistrx("Artist:</div>[^>]*>[^>]*>([^<]*)");
    QRegularExpression statusrx("Pub. status:</div>[^>]*>([^<]*)");
    QRegularExpression yearrx;
    QRegularExpression demographicrx("Demographic:</div>[^>]*>[^>]*>[^>]*>([^<]*)<");
    QRegularExpression genresrx("Genre:</div>[^>]*>[^>]*>([^<]*)<");

    QRegularExpression summaryrx("Description:</div>[^>]*>(.*?)</div>");

    QRegularExpression coverrx(R"lit(<img class="rounded" width="100%" src="([^\"]*)")lit");

    QRegularExpression chapterrx("<a href='(/chapter/[^']*)'[^>]*>([^<]*)</a>");

    QRegularExpression pagerx(R"(<p class='text-center'>Showing 1 to \d+ of ([\d,]+))");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx, yearrx, genresrx, summaryrx,
                  coverrx);

    auto demographicrxmatch = demographicrx.match(job->buffer);

    if (demographicrxmatch.hasMatch())
    {
        auto demo = htmlToPlainText(demographicrxmatch.captured(1).trimmed());

        if (info->genres != "")
            demo += ", ";
        else
            demo += " ";

        info->genres = demo + info->genres;
    }

    int pages = 1;

    auto pagerxmatch = pagerx.match(job->buffer);
    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        for (auto &match : getAllRxMatches(chapterrx, job->buffer))
        {
            info->chapters.insert(0, MangaChapter(match.captured(2), baseurl + match.captured(1)));
        }
    };

    lambda(job);

    if (pagerxmatch.hasMatch())
    {
        int chapters = pagerxmatch.captured(1).remove(',').toInt();
        pages = (chapters + 99) / 100;

        QList<QString> urls;
        for (int i = 2; i <= pages; i++)
            urls.append(info->link + QString::number(i));

        DownloadQueue queue(networkManager, urls, CONF.parallelDownloadsLow, lambda, false);
        queue.start();
        queue.awaitCompletion();
    }
}

Result<QStringList, QString> MangaDex::getPageList(const QString &chapterlink)
{
    QString scriptstart("<script");
    QRegularExpression baserx(R"(server\s+=\s+'([^']*)')");
    QRegularExpression datarx("var dataurl = '([^']*)'");
    QRegularExpression pagesrx("var page_array = \\[([^\\]]*)");

    auto job = networkManager->downloadAsString(chapterlink);

    if (!job->await(7000))
        return Err(job->errorString);

    int spos = job->buffer.indexOf(scriptstart);
    if (spos == -1)
        return Err(QString("Error. Couldn't process pages/images."));

    auto baserxmatch = baserx.match(job->buffer);
    auto datarxmatch = datarx.match(job->buffer);
    auto pagesrxmatch = pagesrx.match(job->buffer);

    if (!baserxmatch.hasMatch() || !datarxmatch.hasMatch() || !pagesrxmatch.hasMatch())
        return Err(QString("Error. Couldn't process pages/images."));

    QString baselink = baserxmatch.captured(1).remove('\\') + datarxmatch.captured(1) + '/';

    QStringList imagelinks;
    for (QString s : pagesrxmatch.captured(1).split(','))
    {
        s = s.remove('\'').remove('\r').remove('\n');
        if (s != "")
            imagelinks.append(baselink + s.remove('\'').remove('\r'));
    }

    return Ok(imagelinks);
}
