#include "jaiminisbox.h"

#include <QElapsedTimer>
#include <QMessageBox>

#include "configs.h"

JaiminisBox::JaiminisBox(QObject *parent, DownloadManager *dm)
    : AbstractMangaSource(parent)
{
    nummangas = 0;
    name = "JaiminisBox";
    baseurl = "https://jaiminisbox.com/";
    downloadmanager = dm;
}

bool JaiminisBox::updateMangaList()
{
    QElapsedTimer timer;
    timer.start();

    QString nextlink = baseurl + "/reader/directory/";

    emit updateProgress(30);

    int rem = 70;

    QRegExp rx(R"lit(<div class="title"><a href="([^"]*)"[^"]*"([^"]*)")lit");
    QRegExp nextrx("<a class=\"gbutton fright\" href=\"([^\"]*)\">Next");

    mangalist.links.clear();
    mangalist.titles.clear();
    nummangas = 0;

    do
    {
        DownloadStringJob *job =
            downloadmanager->downloadAsString(nextlink, -1);
        if (!job->await(5000))
        {
            emit updateError(job->errorString);
            return false;
        }

        int pos = 0;
        while ((pos = rx.indexIn(job->buffer, pos)) != -1)
        {
            pos += rx.matchedLength();
            mangalist.links.append(rx.cap(1).mid(baseurl.length()));
            mangalist.titles.append(htmlToPlainText(rx.cap(2)));

            //            qDebug() << rx.cap(1) << htmlToPlainText(rx.cap(2));
            nummangas++;
        }

        if (nextrx.indexIn(job->buffer, 0) != -1)
            nextlink = nextrx.cap(1);
        else
            nextlink = "";

        rem = rem / 2;
        emit updateProgress(100 - rem);
        delete job;
    } while (nextlink != "");

    qDebug() << nummangas;

    emit updateProgress(100);

    return true;
}

MangaInfo *JaiminisBox::getMangaInfo(QString mangalink)
{
    //    qDebug() << mangalink;
    DownloadStringJob *job = downloadmanager->downloadAsString(mangalink);

    MangaInfo *info = new MangaInfo(this, this);
    info->mangasource = this;
    info->hostname = name;

    info->link = mangalink;

    QRegExp titlerx(R"(<h1 class="title">\s+([^<]*))");
    QRegExp authorrx("Author</b>:([^<]*)");
    QRegExp artistrx("Artist</b>:([^<]*)");

    QRegExp summaryrx("Synopsis</b>:(.*)</div>");
    summaryrx.setMinimal(true);

    if (!job->await(3000))
    {
        qDebug() << job->errorString;
        return info;
    }

    int spos = 0;

    if (titlerx.indexIn(job->buffer, spos) != -1)
        info->title = htmlToPlainText(titlerx.cap(1)).trimmed();
    if (authorrx.indexIn(job->buffer, spos) != -1)
        info->author = htmlToPlainText(authorrx.cap(1)).trimmed();
    if (artistrx.indexIn(job->buffer, spos) != -1)
        info->artist = htmlToPlainText(artistrx.cap(1)).trimmed();
    if (summaryrx.indexIn(job->buffer, spos) != -1)
        info->summary = htmlToPlainText(summaryrx.cap(1)).trimmed();

    info->genres = "-";
    info->status = "-";
    info->releaseyear = "-";

    QRegExp coverrx(R"(<div class="thumbnail">[^"]*"([^"]*))");
    //    coverrx.setMinimal(true);

    QString coverlink;
    if (coverrx.indexIn(job->buffer, spos) != -1) coverlink = coverrx.cap(1);

    //    qDebug() << coverlink;

    //    coverlink = coverlink.replace("http:", "https:");
    info->coverlink = coverlink;

    int ind = coverlink.indexOf('?');
    if (ind == -1) ind = coverlink.length();
    QString filetype = coverlink.mid(ind - 4, 4);
    info->coverpath = mangainfodir(name, info->title) + "cover" + filetype;

    DownloadFileJob *coverjob =
        downloadmanager->downloadAsFile(coverlink, info->coverpath);

    QRegExp rx("<div class=\"title\"><a href=\"([^\"]*)\"\\s+title=\"([^\"]*)");

    info->numchapters = 0;
    int pos = 0;
    while ((pos = rx.indexIn(job->buffer, pos)) != -1)
    {
        pos += rx.matchedLength();

        info->chapters.insert(0, MangaChapter(rx.cap(1), this));

        QString ctitle = rx.cap(2);
        info->chapertitlesreversed.append(ctitle);
        info->numchapters++;
        //        qDebug() << info->chapters[0].chapterlink;
        //        qDebug() << rx.cap(2);
    }

    if (coverlink != "" && !coverjob->await(3000))
    {
        //        info->coverpath = "";
    }

    delete job;

    info->serialize();

    return info;
}

void JaiminisBox::updateMangaInfoFinishedLoading(DownloadStringJob *job,
                                                 MangaInfo *info)
{
    int numchapters = job->buffer.count("<div class=\"title\"><a");

    if (numchapters == 0 || numchapters == info->numchapters)
    {
        info->sendUpdated(false);
        return;
    }

    QRegExp rx("<div class=\"title\"><a href=\"([^\"]*)\"\\s+title=\"([^\"]*)");

    info->chapters.clear();
    info->chapertitlesreversed.clear();
    info->numchapters = 0;

    int pos = 0;
    while ((pos = rx.indexIn(job->buffer, pos)) != -1)
    {
        pos += rx.matchedLength();

        info->chapters.insert(0, MangaChapter(rx.cap(1), this));

        QString ctitle = rx.cap(2);
        info->chapertitlesreversed.append(ctitle);
        info->numchapters++;
    }

    info->serialize();

    info->sendUpdated(true);
}

QStringList JaiminisBox::getPageList(const QString &chapterlink)
{
    DownloadStringJob *job = downloadmanager->downloadAsString(chapterlink, -1);
    QStringList pageLinks;

    if (!job->await(4000))
    {
        qDebug() << "getPageList: download error.";
        return pageLinks;
    }

    //    qDebug() << chapterlink;

    QRegExp rx(R"(JSON.parse\(atob\("([^"]*))");

    if (rx.indexIn(job->buffer, 0) == -1) return pageLinks;

    QByteArray decoded = QByteArray::fromBase64(rx.cap(1).toLatin1());
    QString decodedstr(decoded);

    //    qDebug() << decodedstr;

    QRegExp pagerx(R"("url":"([^"]*))");

    int pos = 0;
    while ((pos = pagerx.indexIn(decodedstr, pos)) != -1)
    {
        pos += pagerx.matchedLength();

        pageLinks.append(pagerx.cap(1).replace("\\/", "/"));
        //        qDebug() << pagerx.cap(1).replace("\\/", "/");
    }

    delete job;
    return pageLinks;
}

QString JaiminisBox::getImageLink(const QString &pagelink) { return pagelink; }
