#include "jaiminisbox.h"

#include <QElapsedTimer>
#include <QMessageBox>

#include "defines.h"

JaiminisBox::JaiminisBox(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent, dm)
{
    name = "JaiminisBox";
    baseurl = "https://jaiminisbox.com/";

    QUrlQuery postdata;
    postdata.addQueryItem("adult", "true");
    postdatastr = postdata.query().toUtf8();
}

MangaList JaiminisBox::getMangaList()
{
    QRegularExpression mangarx(
        R"lit(<div class="title"><a href="([^"]*)"[^"]*"([^"]*)")lit");
    QRegularExpression nextpagerx(
        "<a class=\"gbutton fright\" href=\"([^\"]*)\">Next");

    MangaList mangas;

    QString nextlink = baseurl + "/reader/directory/";

    emit updateProgress(30);

    QElapsedTimer timer;
    timer.start();

    int rem = 70;

    do
    {
        auto job = downloadmanager->downloadAsString(nextlink);
        if (!job->await(5000))
        {
            emit updateError(job->errorString);
            return mangas;
        }

        for (auto &match : getAllRxMatches(mangarx, job->buffer))
        {
            mangas.links.append(match.captured(1));
            mangas.titles.append(htmlToPlainText(match.captured(2)));
            mangas.actualSize++;
        }

        auto nextpagematch = nextpagerx.match(job->buffer);

        if (nextpagematch.hasMatch())
            nextlink = nextpagematch.captured(1);
        else
            nextlink = "";

        rem = rem / 2;
        emit updateProgress(100 - rem);
    } while (nextlink != "");

    mangas.nominalSize = mangas.actualSize;
    mangas.absoluteUrls = true;

    qDebug() << "update completed. mangas:" << mangas.actualSize
             << "time:" << timer.elapsed();

    emit updateProgress(100);

    return mangas;
}

QSharedPointer<MangaInfo> JaiminisBox::getMangaInfo(const QString &mangalink)
{
    auto job = downloadmanager->downloadAsStringPost(mangalink, &postdatastr);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;

    if (!job->await(3000))
        return info;

    updateMangaInfoFinishedLoading(job, info);

    downloadCover(info);

    return info;
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

    for (auto &chapterrxmatch : getAllRxMatches(chapterrx, job->buffer))
    {
        info->chapters.insert(0,
                              MangaChapter(chapterrxmatch.captured(1), this));
        info->chapertitlesreversed.append(chapterrxmatch.captured(2));
        info->numchapters++;
    }
}

QStringList JaiminisBox::getPageList(const QString &chapterlink)
{
    QRegularExpression encodedrx(R"(JSON.parse\(atob\("([^"]*))");
    QRegularExpression imagelinksrx(R"("url":"([^"]*))");

    auto job =
        downloadmanager->downloadAsStringPost(chapterlink, &postdatastr, -1);
    QStringList imagelinks;

    if (!job->await(4000))
        return imagelinks;

    auto rxmatch = encodedrx.match(job->buffer);
    if (!rxmatch.hasMatch())
        return imagelinks;

    QByteArray decoded = QByteArray::fromBase64(rxmatch.captured(1).toLatin1());
    QString decodedstr(decoded);

    for (auto &match : getAllRxMatches(imagelinksrx, decodedstr))
    {
        imagelinks.append(match.captured(1).replace("\\/", "/"));
    }

    return imagelinks;
}

QString JaiminisBox::getImageLink(const QString &pagelink)
{
    // pagelinks are actually already imagelinks
    return pagelink;
}
