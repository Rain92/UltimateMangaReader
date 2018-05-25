#include "mangadex.h"

#include "configs.h"
#include "QTime"

MangaDex::MangaDex(QObject *parent, DownloadManager *dm):
    AbstractMangaSource(parent)
{
    AbstractMangaSource::nummangas = 0;
    AbstractMangaSource::name = "MangaDex";
    AbstractMangaSource::baseurl = "https://mangadex.org";
    AbstractMangaSource::downloadmanager = dm;
}

bool MangaDex::updateMangaList()
{
    QTime timer;
    timer.start();

//    downloadmanager->addCookie("mangadex.org", "mangadex_h_toggle", "1");
    downloadmanager->addCookie("mangadex.org", "mangadex_title_mode", "2");
    downloadmanager->addCookie("mangadex.org", "mangadex_filter_langs", "1");

    QString basedictlink = AbstractMangaSource::baseurl + "/titles/2/";

    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(basedictlink + "1");

    if (!job->await(5000))
        return false;

    mangalist.links.clear();
    mangalist.titles.clear();

//    qDebug() << "time" << timer.elapsed();


    QRegExp chsstart("<p class='text-center'>Showing");
    QRegExp chsrx(" of (\\d{2,3},\\d{3})");
    int cpos = chsstart.indexIn(job->buffer, 0) + chsrx.matchedLength() + 1;
    chsrx.indexIn(job->buffer, cpos);
    nummangas = chsrx.cap(1).remove(',').toInt();

    int pages = (nummangas + 99) / 100;

//    qDebug() << pages;

    QVector<DownloadStringJob *> jobs = QVector<DownloadStringJob *>(pages);
    jobs[0] = job;


    QRegExp rx("<a class='' title='([^']*)' href='([^']*)'");

    for (int batch = 0, dli = 1, rxi = 0; batch < (pages + maxparalleldownloads - 1) / maxparalleldownloads; batch++)
    {
        for (; dli < (batch + 1) * maxparalleldownloads && dli < pages; dli++)
        {
            jobs[dli] = AbstractMangaSource::downloadmanager->downloadAsString(basedictlink + QString::number(dli + 1));
//            qDebug() << "dl" << dli << "time:" << timer.elapsed();
        }
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {
            if (!jobs[rxi]->await(10000, true))
                return false;

            for (int pos = 0; (pos = rx.indexIn(jobs[rxi]->buffer, pos)) != -1; pos += rx.matchedLength())
            {
                mangalist.links.append(rx.cap(2));
                mangalist.titles.append(rx.cap(1));
            }

//            qDebug() << "rx" << rxi << "time:" << timer.elapsed();
            delete jobs[rxi];
        }
        emit progress(100 * rxi / pages);
    }


    emit progress(100);
//    qDebug() << "time" << timer.elapsed();
    return true;
}


MangaInfo *MangaDex::getMangaInfo( QString mangalink)
{
    downloadmanager->addCookie("mangadex.org", "mangadex_filter_langs", "1");

    if (mangalink.left(5) != "https")
        mangalink = AbstractMangaSource::baseurl + mangalink;
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(mangalink);

    MangaInfo *info = new MangaInfo(this, this);
    info->mangasource = this;
    info->hostname = AbstractMangaSource::name;

    info->link = mangalink;


    QRegExp titlerx("<h3 class=\"panel-title\">[^>]*>[^>]*>([^<]*)");
    QRegExp authorrx("Author:</th>[^>]*>[^>]*>([^<]*)");
    QRegExp artistrx("Artist:</th>[^>]*>[^>]*>([^<]*)");
    QRegExp statusrx("Pub. status:</th>[^>]*>([^<]*)");

    QRegExp summaryrx("Description:</th>[^>]*>(.*)</td>");
    summaryrx.setMinimal(true);


    if (!job->await(3000))
    {
        qDebug() << job->errorString;
        return info;
    }

    int spos = job->buffer.indexOf("<div class=\"container\" role=\"main\">");

    if (titlerx.indexIn(job->buffer, spos) != -1)
        info->title = titlerx.cap(1).trimmed();
    if (authorrx.indexIn(job->buffer, spos) != -1)
        info->author = authorrx.cap(1);
    if (artistrx.indexIn(job->buffer, spos) != -1)
        info->artist = artistrx.cap(1);
    if (statusrx.indexIn(job->buffer, spos) != -1)
        info->status = statusrx.cap(1);
    if (summaryrx.indexIn(job->buffer, spos) != -1)
        info->summary = summaryrx.cap(1);
    info->summary = info->summary.remove("&quot;").remove("<br />");

    info->releaseyear = "";

    QRegExp coverrx("src='([^']*)[^>]*title='Manga image'");

    QString coverlink;
    if (coverrx.indexIn(job->buffer, spos) != -1)
        coverlink = AbstractMangaSource::baseurl + coverrx.cap(1);


    int ind = coverlink.indexOf('?');
    if (ind == -1)
        ind = coverlink.length();
    QString filetype = coverlink.mid(ind - 4, 4);
    coverlink = coverlink.replace("http:", "https:");
    info->coverpath = mangainfodir(name, info->title) + "cover" + filetype;


    DownloadFileJob *coverjob = AbstractMangaSource::downloadmanager->downloadAsFile(coverlink, info->coverpath);


    QRegExp rx("data-chapter-name=\"[^\"]*\" href=\"(/chapter/[^\"]*)\">([^<]*)</a>");

    info->numchapters = 0;

    QRegExp multipagerx("<p class='text-center'>Showing 1 to 100 of (\\S+)");

    int pages = 1;

    if (multipagerx.indexIn(job->buffer, spos) != -1)
    {
        int chapters = multipagerx.cap(1).remove(',').toInt();
        pages = (chapters + 99) / 100;

    }
    QVector<DownloadStringJob *> jobs = QVector<DownloadStringJob *>(pages);
    jobs[0] = job;


    for (int batch = 0, dli = 1, rxi = 0; batch < (pages + maxparalleldownloads - 1) / maxparalleldownloads; batch++)
    {
//        qDebug() << "batch" << batch;
//        qDebug() << "dli" << dli;
//        qDebug() << "rxi" << rxi;
        for (; dli < (batch + 1) * maxparalleldownloads && dli < pages; dli++)
        {
            jobs[dli] = AbstractMangaSource::downloadmanager->downloadAsString(mangalink + "/chapters/" + QString::number(dli + 1));
//            qDebug() << "dl" << dli;
        }
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {
            if (!jobs[rxi]->await(6000, true))
                return info;

            for (int pos = 0; (pos = rx.indexIn(jobs[rxi]->buffer, pos)) != -1; pos += rx.matchedLength())
            {
                info->chapters.insert(0, MangaChapter(AbstractMangaSource::baseurl + rx.cap(1), this));

                info->chapertitlesreversed.append(rx.cap(2));
                info->numchapters++;
            }

//            qDebug() << "rx" << rxi ;
            delete jobs[rxi];
        }
    }

    if (coverlink != "" && !coverjob->await(3000))
    {

        info->coverpath = "";
    }

    return info;
}

QStringList *MangaDex::getPageList(const QString &chapterlink)
{
    DownloadStringJob *job = AbstractMangaSource::downloadmanager->downloadAsString(chapterlink);
    QStringList *pageLinks = new QStringList();

    if (!job->await(3000))
        return pageLinks;


    QRegExp rx("<script data-type=\"chapter\">");
    rx.setMinimal(true);

    int spos = rx.indexIn(job->buffer, 0);
    if (spos == -1)
        return pageLinks;

    QRegExp baserx("\"server\":\"([^\"]*)");
    QRegExp datarx("\"dataurl\":\"([^\"]*)");
    QRegExp pagesrx("\"page_array\":\\[([^\\]]*)");

    if (baserx.indexIn(job->buffer, spos) == -1)
        return pageLinks;

    if (datarx.indexIn(job->buffer, spos) == -1)
        return pageLinks;

    QString baselink = baserx.cap(1).remove('\\') + datarx.cap(1) + '/';


    if ( pagesrx.indexIn(job->buffer, spos) == -1)
        return pageLinks;


    foreach (QString s, pagesrx.cap(1).split(','))
    {
        pageLinks->append(baselink + s.mid(1, s.length() - 2));
    }

    delete job;
    return pageLinks;
}


QString MangaDex::getImageLink(const QString &pagelink)
{
    return pagelink;
}

