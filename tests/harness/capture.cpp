// The output of the sources, as text that is compared with tests/golden.
#include <QCryptographicHash>
#include <QFile>
#include <QImage>

#include "harness.h"
#include "mangarepository.h"
#include "mangasourcebase.h"

static QString shorten(const QString &s, int n = 100)
{
    QString t = s;
    t.replace('\n', "\\n");
    return t.size() > n ? t.left(n) + "..." : t;
}

// in the format of the old page urls (url#vwt=<token>|xor:<key>|append:<url>)
static QString linkString(const ImageLink &link)
{
    QString s = link.url;
    for (const auto &h : link.headers)
        if (h.name == "Plus-Vw-Token")
            s += "#vwt=" + QString::fromLatin1(h.value);
    if (!link.xorKey.isEmpty())
        s += "|xor:" + QString::fromLatin1(link.xorKey.toHex());
    if (!link.appendUrl.isEmpty())
        s += "|append:" + link.appendUrl;
    return s;
}

static QString pixelHash(const QImage &image)
{
    auto grey = image.convertToFormat(QImage::Format_Grayscale8);
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (int y = 0; y < grey.height(); y++)
        hash.addData(QByteArrayView(grey.constScanLine(y), grey.width()));
    return QString::fromLatin1(hash.result().toHex().left(16));
}

static int captureImage(MangaRepository &repository, MangaSourceBase *source, const QString &path,
                        const ImageLink &image)
{
    auto job = repository.downloadImage(source->name, image, path);
    if (!job)
    {
        out << "      image: no job\n";
        return 1;
    }
    bool ok = job->await(30000);
    QImage img(job->filepath);
    out << "      image: " << (ok ? "ok" : "FAILED " + job->error.message) << " " << img.width() << "x"
        << img.height() << "\n";
    if (ok)
        out << "      pixels: " << pixelHash(img) << "\n";
    return ok ? 0 : 1;
}

static int capturePages(MangaRepository &repository, MangaSourceBase *source, const QString &mangaUrl,
                        const MangaChapterCollection &chapters, int c, bool images)
{
    int failures = 0;
    out << "  chapter " << c << ": " << chapters[c].chapterTitle << " | " << chapters[c].chapterUrl << "\n";
    auto pages = source->fetchPageList(chapters[c].chapterUrl).wait();
    if (!pages.has_value())
    {
        out << "    pages: ERROR " << pages.error().message << "\n";
        return 1;
    }

    out << "    pages: " << pages->size() << "\n";
    QList<int> picked;
    for (int p : {0, 1, int(pages->size()) - 1})
    {
        if (p < 0 || p >= pages->size() || picked.contains(p))
            continue;
        picked.append(p);
        out << "    page " << p << ": " << linkString((*pages)[p]) << "\n";
        auto img = source->resolveImage((*pages)[p]).wait();
        if (!img.has_value())
        {
            out << "      imageUrl: ERROR " << img.error().message << "\n";
            failures++;
            continue;
        }
        out << "      imageUrl: " << linkString(img.value()) << "\n";

        // the tokens are made up per session
        QStringList headers;
        for (const auto &h : repository.imageDownload(source->name, img.value(), "").image.headers)
            headers << QString::fromLatin1(h.name + ": " +
                                           (h.name == "Plus-Vw-Token" || h.name == "SESSION-TOKEN"
                                                ? QByteArray("<token>")
                                                : h.value));
        out << "      headers: " << headers.join(" | ") << "\n";
        if (images && p == 0)
        {
            auto path = CONF.mangaimagesdir(source->name, mangaUrl) + CONF.pagefilename(chapters[c].chapterUrl, p);
            failures += captureImage(repository, source, path, img.value());
        }
    }
    return failures;
}

int captureMangas(MangaRepository &repository, MangaSourceBase *source, const QString &picksFile)
{
    QFile file(picksFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        out << "can't read " << picksFile << "\n";
        return 1;
    }

    int failures = 0;
    const auto lines = QString::fromUtf8(file.readAll()).split('\n');
    for (const auto &line : lines)
    {
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        auto parts = line.split('\t');
        if (parts.size() != 2)
            continue;
        const auto &title = parts[0], &url = parts[1];

        out << "=== " << title << " | " << url << "\n";
        out.flush();
        auto res = source->fetchMangaInfo(url, {}).wait();
        if (!res.has_value())
        {
            out << "  info: ERROR " << res.error().message << "\n";
            failures++;
            continue;
        }
        const auto &info = res.value();
        out << "  title: " << title << "\n  url: " << url << "\n  author: " << info.author
            << "\n  artist: " << info.artist << "\n  status: " << info.status << "\n  year: " << info.releaseYear
            << "\n  genres: " << info.genres << "\n  summary: " << shorten(info.summary)
            << "\n  coverUrl: " << info.coverUrl << "\n  chapters: " << info.chapters.size() << "\n";

        // all of the summary and the chapters
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(info.summary.toUtf8());
        int n = info.chapters.size();
        for (int c = 0; c < n; c++)
        {
            hash.addData(("\n" + info.chapters[c].chapterTitle + "\t" + info.chapters[c].chapterUrl).toUtf8());
            if (c < 3 || c >= n - 3)
                out << "  ch " << c << ": " << info.chapters[c].chapterTitle << " | "
                    << info.chapters[c].chapterUrl << "\n";
        }
        out << "  hash: " << hash.result().toHex().left(16) << "\n";
        if (n > 0)
            failures += capturePages(repository, source, url, info.chapters, 0, true);
        if (n > 1)
            failures += capturePages(repository, source, url, info.chapters, n - 1, false);
        out.flush();
    }
    return failures;
}

int captureList(MangaSourceBase *source, MangaList *result)
{
    UpdateProgressToken token;
    token.currentSourceName = source->name;
    token.sourcesProgress.insert(source->name, 0);

    auto update = source->fetchMangaList(&token, MangaList()).wait();
    const auto &list = update.list;
    QString status = update.status == MangaListUpdate::Complete  ? QString("ok")
                     : update.status == MangaListUpdate::Partial ? "partial " + update.error.message
                                                                 : "FAILED " + update.error.message;
    out << "=== list " << source->name << "\n  update: " << status << "\n  entries: " << list.size
        << ", absolute urls " << list.absoluteUrls << "\n";

    // all entries, e.g. to compare two updates. The urls are absolute (the ones the app uses), sources that keep
    // them relative to different base urls compare equal.
    QFile dump(qEnvironmentVariable("HARNESS_LIST_DUMP"));
    if (!dump.fileName().isEmpty() && !dump.open(QIODevice::WriteOnly))
        qWarning() << "Couldn't write" << dump.fileName();

    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (int i = 0; i < list.titles.size(); i++)
    {
        auto url = list.absoluteUrls ? list.urls.value(i) : source->baseUrl + list.urls.value(i);
        QString entry = list.titles[i] + "\t" + url + "\t" + QString::number(list.popularityRanks.value(i)) + "\n";
        hash.addData(entry.toUtf8());
        if (dump.isOpen())
            dump.write(entry.toUtf8());
        if (i < 5 || i >= list.titles.size() - 5)
            out << "  " << i << ": " << list.titles[i] << " | " << url << " | rank "
                << list.popularityRanks.value(i) << "\n";
    }
    out << "  hash: " << hash.result().toHex().left(16) << "\n";
    if (result)
        *result = list;
    return update.status == MangaListUpdate::Complete ? 0 : 1;
}
