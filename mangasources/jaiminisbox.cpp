#include "jaiminisbox.h"

JaiminisBox::JaiminisBox(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "JaiminisBox";
    baseurl = "https://jaiminisbox.com/";

    QUrlQuery postdata;
    postdata.addQueryItem("adult", "true");
    mangaInfoPostDataStr = postdata.query().toUtf8();
}

MangaList JaiminisBox::getMangaList()
{
    QRegularExpression mangarx(
        R"lit(<div class="title"><a href="([^"]*)"[^"]*"([^"]*)")lit");

    QRegularExpression numpagesrx(R"(/directory/(\d+)/">Last)");

    MangaList mangas;

    QString readerlink = baseurl + "reader/directory/";

    auto job = downloadManager->downloadAsString(readerlink + "1", -1);

    if (!job->await(7000))
    {
        emit updateError(job->errorString);
        return mangas;
    }

    emit updateProgress(10);

    QElapsedTimer timer;
    timer.start();

    auto numpagesrxmatch = numpagesrx.match(job->buffer);

    int pages = 1;
    if (numpagesrxmatch.hasMatch())
        pages = numpagesrxmatch.captured(1).toInt();

    qDebug() << "pages" << pages;
    const int matchesPerPage = 36;

    auto lambda = [&](QSharedPointer<DownloadStringJob> job) {
        int matches = 0;
        for (auto &match : getAllRxMatches(mangarx, job->buffer))
        {
            mangas.links.append(match.captured(1));
            mangas.titles.append(htmlToPlainText(match.captured(2)).trimmed());
            matches++;
        }
        mangas.actualSize += matches;

        emit updateProgress(10 +
                            90 * (mangas.actualSize / matchesPerPage) / pages);

        qDebug() << "matches:" << matches;
    };

    lambda(job);

    QList<QString> urls;
    for (int i = 2; i <= pages; i++)
        urls.append(readerlink + QString::number(i));

    DownloadQueue queue(downloadManager, urls, CONF.parallelDownloadsLow,
                        lambda);

    queue.start();

    awaitSignal(&queue, {SIGNAL(allCompleted())}, 1700000);

    mangas.nominalSize = mangas.actualSize;
    mangas.absoluteUrls = true;

    qDebug() << "mangas:" << mangas.actualSize << "time:" << timer.elapsed();

    emit updateProgress(100);

    return mangas;
}

void JaiminisBox::updateMangaInfoFinishedLoading(
    QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info)
{
    QRegularExpression titlerx(R"(<h1 class="title">\s+([^<]*))");
    QRegularExpression authorrx("Author</b>:([^<]*)");
    QRegularExpression artistrx("Artist</b>:([^<]*)");
    QRegularExpression statusrx;
    QRegularExpression yearrx;
    QRegularExpression genresrx;
    QRegularExpression summaryrx("Synopsis</b>:(.*)</div>");
    QRegularExpression coverrx(R"(<div class="thumbnail">[^"]*"([^"]*))");

    QRegularExpression chapterrx(
        "<div class=\"title\"><a href=\"([^\"]*)\"\\s+title=\"([^\"]*)");

    fillMangaInfo(info, job->buffer, titlerx, authorrx, artistrx, statusrx,
                  yearrx, genresrx, summaryrx, coverrx);

    MangaChapterCollection newchapters;
    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer))
        newchapters.insert(0, MangaChapter(chapterrxmatch.captured(2),
                                           chapterrxmatch.captured(1)));
    info->chapters.mergeChapters(newchapters);
}

Result<QStringList, QString> JaiminisBox::getPageList(
    const QString &chapterlink)
{
    QRegularExpression encodedrx(R"(JSON.parse\(atob\("([^"]*))");
    QRegularExpression imagelinksrx(R"("url":"([^"]*))");

    auto job = downloadManager->downloadAsString(chapterlink, 6000,
                                                 mangaInfoPostDataStr);

    if (!job->await(7000))
        return Err(job->errorString);

    auto rxmatch = encodedrx.match(job->buffer);
    if (!rxmatch.hasMatch())
        return Err(QString("Error. Couldn't process pages/images."));

    QByteArray decoded = QByteArray::fromBase64(rxmatch.captured(1).toLatin1());
    QString decodedstr(decoded);

    QStringList imageLinks;
    for (auto &match : getAllRxMatches(imagelinksrx, decodedstr))
    {
        imageLinks.append(match.captured(1).replace("\\/", "/"));
    }

    return Ok(imageLinks);
}
