// The test harness of the core, see tests/README.md.
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>

#include "abstractmangasource.h"
#include "fixtures.h"
#include "harness.h"
#include "mangachapterdownloadmanager.h"
#include "mangainfo.h"
#include "networkmanager.h"
#include "settings.h"
#include "sourceregistry.h"
#include "staticsettings.h"

QTextStream out(stdout);

AbstractMangaSource *findSource(const QList<QSharedPointer<AbstractMangaSource>> &sources, const QString &name)
{
    for (const auto &source : sources)
        if (source->name == name)
            return source.get();
    return nullptr;
}

static int usage()
{
    out << "usage: harness [--cache <dir>] [--record <dir> | --replay <dir>] <command>\n"
           "  capture <source> <picks file>   the infos, chapters, pages and images of the picked mangas\n"
           "  list <source>                   the update of the manga list\n"
           "  pick <source>                   updates the list, prints mangas for a picks file\n"
           "  download <source> <title> <url> [chapters]   the chapter downloads (download manager)\n"
           "  jobs <port>                     the download jobs, against testserver.py\n"
           "  storage <port>                  the storage of the app, against testserver.py\n"
           "--record writes the responses of the sites into the directory, --replay answers the requests from\n"
           "there. The cache is a temporary directory unless --cache is given.\n";
    return 2;
}

// The picks: the most popular manga and two others further down the list.
static int pick(AbstractMangaSource *source)
{
    int failures = captureList(source);
    const auto &list = source->mangaList;
    QList<int> order(list.titles.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return list.popularityRanks[a] < list.popularityRanks[b]; });
    out << "# picks\n";
    for (int rank : {0, 7, 60})
        if (rank < order.size())
            out << list.titles[order[rank]] << "\t"
                << (list.absoluteUrls ? QString() : source->baseUrl) + list.urls[order[rank]] << "\n";
    return failures;
}

static int download(NetworkManager &nm, AbstractMangaSource *source, const QString &title, const QString &url,
                    int count)
{
    auto res = source->loadMangaInfo(url, title, false);
    if (res.isErr())
    {
        out << "info: ERROR " << res.unwrapErr() << "\n";
        return 1;
    }
    auto info = res.unwrap();
    count = qMin(count, int(info->chapters.size()));

    MangaChapterDownloadManager manager(&nm);
    QObject::connect(&manager, &MangaChapterDownloadManager::error,
                     [](const QString &e) { out << "error: " << e << "\n"; });
    bool done = false;
    int failed = -1;
    QObject::connect(&manager, &MangaChapterDownloadManager::downloadCompleted,
                     [&](int f)
                     {
                         done = true;
                         failed = f;
                     });
    manager.downloadMangaChapters(info, 0, count - 1);
    while (!done)
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);

    out << "failed chapters " << failed << "\n";
    for (int c = 0; c < count; c++)
    {
        int n = info->chapters[c].pages.size(), have = 0;
        for (int p = 0; p < n; p++)
            have += QFile::exists(info->pagePath(c, p));
        out << "chapter " << c << ": " << have << "/" << n << " images\n";
    }
    return failed == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    QStringList args = app.arguments().mid(1);
    QString cacheDir, fixturesDir;
    auto mode = Fixtures::Mode::Live;
    while (args.size() >= 2 && args[0].startsWith("--"))
    {
        auto option = args.takeFirst(), value = args.takeFirst();
        if (option == "--cache")
            cacheDir = value;
        else if (option == "--record" || option == "--replay")
        {
            mode = option == "--record" ? Fixtures::Mode::Record : Fixtures::Mode::Replay;
            fixturesDir = value;
        }
        else
            return usage();
    }
    if (args.isEmpty())
        return usage();

    QTemporaryDir tempDir;
    if (cacheDir.isEmpty())
        cacheDir = tempDir.path() + "/cache";
    CONF.cacheDir = QDir(cacheDir).absolutePath() + "/";
    CONF.mangaListDir = CONF.cacheDir + "mangalists/";
    CONF.screensaverDir = tempDir.path() + "/screensavers/";
    QDir().mkpath(CONF.mangaListDir);

    if (!Fixtures::install(mode, fixturesDir))
        return 2;

    const auto command = args.takeFirst();
    auto arg = [&](int i) { return args.value(i); };

    int failures;
    if (command == "jobs" || command == "storage")
    {
        if (args.isEmpty())
            return usage();
        auto server = "http://127.0.0.1:" + arg(0);
        if (command == "storage")
        {
            failures = storageTests(server);
        }
        else
        {
            NetworkManager nm;
            Settings settings;
            nm.setDownloadSettings(QSize(1404, 1872), &settings);
            failures = jobTests(nm, server);
        }
    }
    else
    {
        NetworkManager nm;
        Settings settings;
        nm.setDownloadSettings(QSize(1404, 1872), &settings);
        SourceRegistry sources(&nm);
        auto source = findSource(sources.all(), arg(0));
        if (!source)
        {
            out << "unknown source " << arg(0) << "\n";
            return 2;
        }

        if (command == "capture" && args.size() == 2)
            failures = captureMangas(source, arg(1));
        else if (command == "list")
            failures = captureList(source);
        else if (command == "pick")
            failures = pick(source);
        else if (command == "download" && args.size() >= 3)
            failures = download(nm, source, arg(1), arg(2), args.size() > 3 ? arg(3).toInt() : 2);
        else
            return usage();
    }
    out.flush();

    if (!Fixtures::save())
    {
        qWarning() << "Couldn't save the fixtures in" << fixturesDir;
        return 2;
    }
    if (!Fixtures::missing().isEmpty())
    {
        qWarning() << Fixtures::missing().size() << "requests weren't in the fixtures";
        return 2;
    }
    return failures == 0 ? 0 : 1;
}
