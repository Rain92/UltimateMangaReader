#include "mangadex.h"

#include "configs.h"
#include "QTime"

MangaDex::MangaDex(QObject *parent, DownloadManager *dm):
    AbstractMangaSource(parent)
{
    nummangas = 0;
    name = "MangaDex";
    baseurl = "https://mangadex.org";
    downloadmanager = dm;

    initialize();
}

void MangaDex::login()
{
    QUrlQuery  postData;
    postData.addQueryItem("login_username", "UMRBot2");
    postData.addQueryItem("login_password", "umrbot123");
    postData.addQueryItem("remember_me", "1");

    QNetworkRequest request(QString("https://mangadex.org/ajax/actions.ajax.php?function=login&nojs=1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QNetworkReply *reply = downloadmanager->networkmanager->post(request, postData.query().toUtf8());



    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer->start(10000);

    loop.exec();
    timer->stop();

    QList<QNetworkCookie> ncookies = reply->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie> >();
    if (ncookies.count() != 0)
    {
        foreach (QNetworkCookie c, ncookies)
        {
            qDebug() << "Added cookie" <<  c.name() << c.value();
            downloadmanager->addCookie(".mangadex.org",  c.name(), c.value());
        }

    }
}


void MangaDex::initialize()
{
    //    downloadmanager->addCookie(".mangadex.org", "mangadex_h_toggle", "1");
    downloadmanager->addCookie(".mangadex.org", "mangadex_title_mode", "2");
    downloadmanager->addCookie(".mangadex.org", "mangadex_filter_langs", "1");
//    downloadmanager->addCookie(".mangadex.org", "mangadex_rememberme_token", "ba07d6d335a2b433d4b57b396d99224cbfaf100cad243a50694161d681270c5a");

    login();
}

bool MangaDex::updateMangaList()
{
    QElapsedTimer timer;
    timer.start();

    QString basedictlink = baseurl + "/titles/2/";

    DownloadStringJob *job = downloadmanager->downloadAsString(basedictlink + "1", -1);

    if (!job->await(5000))
    {
        emit updateError(job->errorString);
        return false;
    }

    mangalist.links.clear();
    mangalist.titles.clear();

//    qDebug() << "time" << timer.elapsed();


    QRegExp chsstart("<p class=[^>]*>Showing");
    QRegExp chsrx(" of (\\d{2,3},\\d{3})");
    int cpos = chsstart.indexIn(job->buffer, 0) + chsrx.matchedLength() + 1;
    chsrx.indexIn(job->buffer, cpos);
    nummangas = chsrx.cap(1).remove(',').toInt();

    int pages = (nummangas + 99) / 100;

//    pages = 10;

    qDebug() << "pages" << pages;

    QVector<DownloadStringJob *> jobs = QVector<DownloadStringJob *>(pages);
    jobs[0] = job;


    QRegExp rx("<a title=['\"]([^'\"]*)['\"] href=['\"]([^'\"]*)['\"] class=['\"][^'\"]*title[^'\"]*['\"]");
    rx.setMinimal(true);

    for (int batch = 0, dli = 1, rxi = 0; batch < (pages + maxparalleldownloads - 1) / maxparalleldownloads; batch++)
    {
        for (; dli < (batch + 1) * maxparalleldownloads && dli < pages; dli++)
        {
            jobs[dli] = downloadmanager->downloadAsString(basedictlink + QString::number(dli + 1), -1);
            qDebug() << "dl" << dli << "time:" << timer.elapsed();
        }
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {

            if (!jobs[rxi]->await(10000, true))
            {
                emit updateError(jobs[rxi]->errorString);
                return false;
            }

            for (int pos = 0; (pos = rx.indexIn(jobs[rxi]->buffer, pos)) != -1; pos += rx.matchedLength())
            {
                mangalist.links.append(rx.cap(2));
                mangalist.titles.append(htmlToPlainText(htmlToPlainText(rx.cap(1))));
                qDebug() << mangalist.links.last();
                qDebug() << mangalist.titles.last();
            }

            qDebug() << "rx" << rxi << "time:" << timer.elapsed();
            if (rxi > 0)
                delete jobs[rxi];
        }
        emit updateProgress(100 * rxi / pages);
    }

    qDebug() << "nummangas" << mangalist.links.count();


    emit updateProgress(100);
//    qDebug() << "time" << timer.elapsed();
    return true;
}


MangaInfo *MangaDex::getMangaInfo(QString mangalink)
{

    if (mangalink.left(5) != "https")
        mangalink = baseurl + mangalink;
    DownloadStringJob *job = downloadmanager->downloadAsString(mangalink);

    MangaInfo *info = new MangaInfo(this, this);
    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;


    QRegExp titlerx("<span class='fas fa-book fa-fw ' aria-hidden='true'></span> <span class=\"mx-1\">([^<]*)<");
    titlerx.setMinimal(true);

    QRegExp authorrx("Author:</div>[^>]*>[^>]*>([^<]*)");
    QRegExp artistrx("Artist:</div>[^>]*>[^>]*>([^<]*)");
    QRegExp statusrx("Pub. status:</div>[^>]*>([^<]*)");
    QRegExp demographicrx("Demographic:</div>[^>]*>[^>]*>[^>]*>([^<]*)<");
    QRegExp genresrx("Genre:</div>[^>]*>[^>]*>([^<]*)<");
    demographicrx.setMinimal(true);
    genresrx.setMinimal(true);

    QRegExp summaryrx("Description:</div>[^>]*>(.*)</div>");
    summaryrx.setMinimal(true);


    if (!job->await(3000))
    {
        qDebug() << "error loading mangainfo" << job->errorString;
        return info;
    }


    int spos = job->buffer.indexOf("<div class=\"container\" role=\"main\"");

    if (titlerx.indexIn(job->buffer, spos) != -1)
        info->title = htmlToPlainText(titlerx.cap(1)).trimmed();
    if (authorrx.indexIn(job->buffer, spos) != -1)
        info->author = htmlToPlainText(authorrx.cap(1));
    if (artistrx.indexIn(job->buffer, spos) != -1)
        info->artist = htmlToPlainText(artistrx.cap(1));
    if (statusrx.indexIn(job->buffer, spos) != -1)
        info->status = statusrx.cap(1);
    if (summaryrx.indexIn(job->buffer, spos) != -1)
        info->summary = htmlToPlainText(summaryrx.cap(1));
    if (demographicrx.indexIn(job->buffer, 0) != -1)
        info->genres = htmlToPlainText(demographicrx.cap(1).trimmed());
    if (genresrx.indexIn(job->buffer, 0) != -1)
    {
        if (info->genres != "")
            info->genres += ", ";
        info->genres += htmlToPlainText(genresrx.cap(1).trimmed());
    }


    info->releaseyear = "-";

    QRegExp coverrx("<img class=\"rounded\" width=\"100%\" src=\"([^\"]*)\"");

    QString coverlink;
    if (coverrx.indexIn(job->buffer, spos) != -1)
        coverlink = baseurl + coverrx.cap(1);

    coverlink = coverlink.replace("http:", "https:");
    info->coverlink = coverlink;
//    qDebug() << coverlink ;


    int ind = coverlink.indexOf('?');
    if (ind == -1)
        ind = coverlink.length();
    QString filetype = coverlink.mid(ind - 4, 4);
    info->coverpath = mangainfodir(name, info->title) + "cover" + filetype;


    DownloadFileJob *coverjob = downloadmanager->downloadAsFile(coverlink, info->coverpath);


    QRegExp rx("<a href='(/chapter/[^']*)'[^>]*>([^<]*)</a>");

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
            jobs[dli] = downloadmanager->downloadAsString(mangalink + "/chapters/" + QString::number(dli + 1));
//            qDebug() << "dl" << dli;
        }
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {
            if (!jobs[rxi]->await(6000, true))
                return info;

            for (int pos = 0; (pos = rx.indexIn(jobs[rxi]->buffer, pos)) != -1; pos += rx.matchedLength())
            {
                info->chapters.insert(0, MangaChapter(baseurl + rx.cap(1), this));

                info->chapertitlesreversed.append(rx.cap(2));
                info->numchapters++;
            }

//            info->chapters.removeLast();
//            info->chapertitlesreversed.removeAt(0);
//            info->numchapters--;

//            qDebug() << "rx" << rxi ;
            jobs[rxi]->deleteLater();
        }
    }


    if (coverlink != "" && !coverjob->await(3000))
    {
//        info->coverpath = "";
    }
//    downloadmanager->fileDownloads->remove(coverlink);

    info->serialize();


    return info;
}



void MangaDex::updateMangaInfoFinishedLoading(DownloadStringJob *job, MangaInfo *info)
{
    QRegExp multipagerx(">Showing 1 to 100 of (\\S+)");

    int oldnumchapters = info->numchapters;
    int numchapters = info->numchapters;

    int pages = 1;
    int spos = 0;

    if (multipagerx.indexIn(job->buffer, spos) != -1)
    {
        numchapters = multipagerx.cap(1).remove(',').toInt();

    }
    else
    {
        numchapters = job->buffer.count("data-chapter=");
    }

    if (numchapters == oldnumchapters)
    {
        info->sendUpdated(false);
        return;
    }



    QRegExp statusrx("Pub. status:</div>[^>]*>([^<]*)");
    if (statusrx.indexIn(job->buffer, spos) != -1)
        info->status = statusrx.cap(1);

    QRegExp rx("data-chapter-name=\"[^\"]*\" href=\"(/chapter/[^\"]*)\">([^<]*)</a>");

    int chapterstoadd = numchapters - oldnumchapters;

    pages = (chapterstoadd + 99) / 100;

    QVector<DownloadStringJob *> jobs = QVector<DownloadStringJob *>(pages);
    jobs[0] = job;

    int insi = 0;
    for (int batch = 0, dli = 1, rxi = 0; batch < (pages + maxparalleldownloads - 1) / maxparalleldownloads; batch++)
    {
//        qDebug() << "batch" << batch;
//        qDebug() << "dli" << dli;
//        qDebug() << "rxi" << rxi;
        for (; dli < (batch + 1) * maxparalleldownloads && dli < pages; dli++)
        {
            jobs[dli] = downloadmanager->downloadAsString(info->link + "/chapters/" + QString::number(dli + 1));
//            qDebug() << "dl" << dli;
        }
        for (; rxi < (batch + 1) * maxparalleldownloads && rxi < pages; rxi++)
        {
            if (!jobs[rxi]->await(6000, true))
            {
                info->sendUpdated(false);
                return;
            }

            for (int pos = 0; (pos = rx.indexIn(jobs[rxi]->buffer, pos)) != -1 && chapterstoadd > 0; pos += rx.matchedLength(), chapterstoadd--)
            {
                info->chapters.insert(oldnumchapters, MangaChapter(baseurl + rx.cap(1), this));

                info->chapertitlesreversed.insert(insi++, rx.cap(2));
                info->numchapters++;
            }

//            qDebug() << "rx" << rxi ;
            if (rxi > 0)
                jobs[rxi]->deleteLater();
        }
    }

    info->serialize();


    info->sendUpdated(true);
}


QStringList MangaDex::getPageList(const QString &chapterlink)
{
//    qDebug() << "chapterlink" << chapterlink ;
    DownloadStringJob *job = downloadmanager->downloadAsString(chapterlink);
    QStringList pageLinks;

    if (!job->await(3000))
        return pageLinks;


    QRegExp rx("<script");
    rx.setMinimal(true);

    int spos = rx.indexIn(job->buffer, 0);
    if (spos == -1)
        return pageLinks;

    QRegExp baserx("server\\s+=\\s+'([^\']*)'");
    QRegExp datarx("var dataurl = '([^']*)'");
    QRegExp pagesrx("var page_array = \\[([^\\]]*)");


    if (baserx.indexIn(job->buffer, spos) == -1)
        return pageLinks;

    if (datarx.indexIn(job->buffer, spos) == -1)
        return pageLinks;

    QString baselink = baserx.cap(1).remove('\\') + datarx.cap(1) + '/';


    if ( pagesrx.indexIn(job->buffer, spos) == -1)
        return pageLinks;


    foreach (QString s, pagesrx.cap(1).split(','))
    {
        s = s.remove('\'').remove('\r').remove('\n');
        if (s != "")
            pageLinks.append(baselink + s.remove('\'').remove('\r'));
    }

    delete job;
    return pageLinks;
}


QString MangaDex::getImageLink(const QString &pagelink)
{
    return pagelink;
}

