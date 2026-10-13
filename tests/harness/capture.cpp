// The output of the sources, as text that is compared with tests/golden.
#include <QCryptographicHash>
#include <QFile>
#include <QImage>

#include "abstractmangasource.h"
#include "harness.h"
#include "mangainfo.h"

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

static int captureImage(AbstractMangaSource *source, QSharedPointer<MangaInfo> info, int c, int p,
                        const ImageLink &image)
{
    auto job = source->downloadImage(image, info->pagePath(c, p));
    if (!job)
    {
        out << "      image: no job\n";
        return 1;
    }
    bool ok = job->await(30000);
    QImage img(job->filepath);
    out << "      image: " << (ok ? "ok" : "FAILED " + job->errorString) << " " << img.width() << "x"
        << img.height() << "\n";
    if (ok)
        out << "      pixels: " << pixelHash(img) << "\n";
    return ok ? 0 : 1;
}

static int capturePages(AbstractMangaSource *source, QSharedPointer<MangaInfo> info, int c, bool images)
{
    int failures = 0;
    out << "  chapter " << c << ": " << info->chapters[c].chapterTitle << " | " << info->chapters[c].chapterUrl
        << "\n";
    auto res = source->updatePageList(info, c);
    if (!res.isOk())
    {
        out << "    pages: ERROR " << res.unwrapErr() << "\n";
        return 1;
    }

    const auto &chapter = info->chapters[c];
    out << "    pages: " << chapter.pages.size() << "\n";
    QList<int> picked;
    for (int p : {0, 1, int(chapter.pages.size()) - 1})
    {
        if (p < 0 || p >= chapter.pages.size() || picked.contains(p))
            continue;
        picked.append(p);
        out << "    page " << p << ": " << linkString(chapter.pages[p].link) << "\n";
        auto img = source->getImageLink(chapter.pages[p].link);
        if (!img.isOk())
        {
            out << "      imageUrl: ERROR " << img.unwrapErr() << "\n";
            failures++;
            continue;
        }
        out << "      imageUrl: " << linkString(img.unwrap()) << "\n";

        // the tokens are made up per session
        QStringList headers;
        for (const auto &h : source->imageDownload(img.unwrap(), "").image.headers)
            headers << QString::fromLatin1(h.name + ": " +
                                           (h.name == "Plus-Vw-Token" || h.name == "SESSION-TOKEN"
                                                ? QByteArray("<token>")
                                                : h.value));
        out << "      headers: " << headers.join(" | ") << "\n";
        if (images && p == 0)
            failures += captureImage(source, info, c, p, img.unwrap());
    }
    return failures;
}

int captureMangas(AbstractMangaSource *source, const QString &picksFile)
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
        auto res = source->getMangaInfo(url, title);
        if (!res.isOk())
        {
            out << "  info: ERROR " << res.unwrapErr() << "\n";
            failures++;
            continue;
        }
        auto info = res.unwrap();
        out << "  title: " << info->title << "\n  url: " << info->url << "\n  author: " << info->author
            << "\n  artist: " << info->artist << "\n  status: " << info->status << "\n  year: " << info->releaseYear
            << "\n  genres: " << info->genres << "\n  summary: " << shorten(info->summary)
            << "\n  coverUrl: " << info->coverUrl << "\n  chapters: " << info->chapters.size() << "\n";

        int n = info->chapters.size();
        for (int c = 0; c < n; c++)
            if (c < 3 || c >= n - 3)
                out << "  ch " << c << ": " << info->chapters[c].chapterTitle << " | "
                    << info->chapters[c].chapterUrl << "\n";
        if (n > 0)
            failures += capturePages(source, info, 0, true);
        if (n > 1)
            failures += capturePages(source, info, n - 1, false);
        out.flush();
    }
    return failures;
}

int captureList(AbstractMangaSource *source)
{
    UpdateProgressToken token;
    token.currentSourceName = source->name;
    token.sourcesProgress.insert(source->name, 0);
    QString error;
    QObject::connect(&token, &UpdateProgressToken::updateError, [&](const QString &e) { error = e; });

    bool ok = source->updateMangaList(&token);
    const auto &list = source->mangaList;
    out << "=== list " << source->name << "\n  update: " << (ok ? "ok" : "FAILED " + error) << "\n  entries: "
        << list.size << ", absolute urls " << list.absoluteUrls << "\n";

    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (int i = 0; i < list.titles.size(); i++)
    {
        QString entry = list.titles[i] + "\t" + list.urls.value(i) + "\t" +
                        QString::number(list.popularityRanks.value(i)) + "\n";
        hash.addData(entry.toUtf8());
        if (i < 5 || i >= list.titles.size() - 5)
            out << "  " << i << ": " << list.titles[i] << " | " << list.urls.value(i) << " | rank "
                << list.popularityRanks.value(i) << "\n";
    }
    out << "  hash: " << hash.result().toHex().left(16) << "\n";
    return ok ? 0 : 1;
}
