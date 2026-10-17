// The time of what reading asks of a source, to compare sources (e.g. two versions of a script, see --sources).
// Meant for --replay: the responses come without waiting, so the times are those of the app.
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <algorithm>

#include "harness.h"
#include "mangarepository.h"
#include "mangasourcebase.h"

namespace
{

struct Step
{
    // info, pages, resolve, image
    QString kind;
    QString what;
    std::function<bool(int round)> run;
    QList<double> ms;
};

double median(QList<double> values)
{
    if (values.isEmpty())
        return 0;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

}  // namespace

int benchReading(MangaRepository &repository, MangaSourceBase *source, const QString &picksFile, int rounds,
                 int imageRounds)
{
    QFile file(picksFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        out << "can't read " << picksFile << "\n";
        return 1;
    }

    // the steps of capture (so the replay has their responses): the info of each manga, the page lists of its
    // first and last chapter, their pages 0, 1 and the last one, the image of page 0 of the first chapter
    QList<Step> steps;
    auto imagesDir = QDir::temp().filePath("bench-images");
    QDir().mkpath(imagesDir);
    for (const auto &line : QString::fromUtf8(file.readAll()).split('\n'))
    {
        auto parts = line.split('\t');
        if (line.startsWith('#') || parts.size() != 2)
            continue;
        auto title = parts[0], url = parts[1];

        auto info = source->fetchMangaInfo(url, {}).wait();
        if (!info)
        {
            out << "info of " << title << ": " << info.error().message << "\n";
            return 1;
        }
        steps.append({"info", title, [source, url](int) { return source->fetchMangaInfo(url, {}).wait().has_value(); }});

        const auto &chapters = info->chapters;
        QList<int> chapterIndexes = {0};
        if (chapters.size() > 1)
            chapterIndexes.append(chapters.size() - 1);
        for (int c : chapterIndexes)
        {
            if (c >= chapters.size())
                continue;
            auto chapterUrl = chapters[c].chapterUrl;
            auto pages = source->fetchPageList(chapterUrl).wait();
            if (!pages)
            {
                out << "pages of " << chapterUrl << ": " << pages.error().message << "\n";
                return 1;
            }
            steps.append({"pages", QString("%1 #%2: %3 pages").arg(title).arg(c).arg(pages->size()),
                          [source, chapterUrl](int) { return source->fetchPageList(chapterUrl).wait().has_value(); }});

            QList<int> picked;
            for (int p : {0, 1, int(pages->size()) - 1})
            {
                if (p < 0 || p >= pages->size() || picked.contains(p))
                    continue;
                picked.append(p);
                auto page = (*pages)[p];
                auto image = source->resolveImage(page).wait();
                if (!image)
                {
                    out << "image of " << page.url << ": " << image.error().message << "\n";
                    return 1;
                }
                steps.append({"resolve", QString("%1 #%2 p%3").arg(title).arg(c).arg(p),
                              [source, page](int) { return source->resolveImage(page).wait().has_value(); }});

                if (c == 0 && p == 0)
                {
                    auto link = *image;
                    auto name = source->name;
                    steps.append({"image", QString("%1 #%2 p%3").arg(title).arg(c).arg(p),
                                  [&repository, name, link, imagesDir](int round)
                                  {
                                      auto path = QString("%1/%2.jpg").arg(imagesDir).arg(round);
                                      QFile::remove(path);
                                      auto job = repository.downloadImage(name, link, path);
                                      bool ok = job && job->await(60000);
                                      QFile::remove(path);
                                      return ok;
                                  }});
                }
            }
        }
    }

    // a round that isn't counted (the first calls of the source), then the measured ones
    int failures = 0;
    QElapsedTimer timer;
    for (int round = 0; round <= rounds; round++)
        for (auto &step : steps)
        {
            if (step.kind == "image" && round > imageRounds)
                continue;
            timer.start();
            bool ok = step.run(round);
            double ms = timer.nsecsElapsed() / 1e6;
            if (!ok)
            {
                out << "FAILED " << step.kind << " " << step.what << "\n";
                failures++;
            }
            if (round > 0)
                step.ms.append(ms);
            else
                step.what += QString(" (first %1 ms)").arg(ms, 0, 'f', 1);
        }

    out << "=== " << source->name << ": the median of " << rounds << " rounds (" << imageRounds
        << " for images), ms\n";
    for (const QString kind : {"info", "pages", "resolve", "image"})
    {
        double sum = 0;
        int count = 0;
        for (const auto &step : steps)
            if (step.kind == kind)
            {
                double ms = median(step.ms);
                out << QString("  %1 %2  %3\n").arg(kind, -8).arg(ms, 8, 'f', 1).arg(step.what);
                sum += ms;
                count++;
            }
        if (count > 0)
            out << QString("  %1 %2  average of %3\n").arg(kind, -8).arg(sum / count, 8, 'f', 1).arg(count);
    }
    QDir(imagesDir).removeRecursively();
    return failures;
}
