// The test harness of the core, see tests/README.md.
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>

#include "comick.h"
#include "fixtures.h"
#include "harness.h"
#include "mangachapterdownloadmanager.h"
#include "mangadex.h"
#include "mangahere.h"
#include "mangainfo.h"
#include "mangakakalot.h"
#include "mangaplus.h"
#include "mangarepository.h"
#include "mangasourcebase.h"
#include "mangatown.h"
#include "networkmanager.h"
#include "settings.h"
#include "sourceregistry.h"
#include "staticsettings.h"

QTextStream out(stdout);

QList<QSharedPointer<MangaSourceBase>> harnessSources(NetworkManager *networkManager, const QString &sourcesDir,
                                                      QStringList *errors)
{
    QList<QSharedPointer<MangaSourceBase>> sources = {
        QSharedPointer<MangaSourceBase>(new MangaDex(networkManager)),
        QSharedPointer<MangaSourceBase>(new Mangakakalot(networkManager)),
        QSharedPointer<MangaSourceBase>(new MangaTown(networkManager)),
        QSharedPointer<MangaSourceBase>(new MangaHere(networkManager)),
        QSharedPointer<MangaSourceBase>(new Comick(networkManager)),
        QSharedPointer<MangaSourceBase>(new MangaPlus(networkManager))};
    if (sourcesDir.isEmpty())
        return sources;

    for (const auto &script : SourceRegistry::loadDirectory(networkManager, sourcesDir, errors))
    {
        auto same = std::find_if(sources.begin(), sources.end(),
                                 [&script](const auto &source) { return source->name == script->name; });
        if (same != sources.end())
            *same = script;
        else
            sources.append(script);
    }
    return sources;
}

static int usage()
{
    out << "usage: harness [--cache <dir>] [--record <dir> | --replay <dir>] [--sources <dir>] <command>\n"
           "  capture <source> <picks file>   the infos, chapters, pages and images of the picked mangas\n"
           "  list <source>                   the update of the manga list\n"
           "  pick <source>                   updates the list, prints mangas for a picks file\n"
           "  download <source> <title> <url> [chapters]   the chapter downloads (download manager)\n"
           "  bench <source> <picks file> [rounds] [image rounds]   the time of reading (info, pages, images)\n"
           "  flow <source> <title> <url>     reading, favorites and downloads through the core like the gui\n"
           "  listflow <source>               list updates (and a cancelled one) through the core like the gui\n"
           "  jobs <port>                     the download jobs, against testserver.py\n"
           "  storage <port>                  the storage of the app, against testserver.py\n"
           "  scripts <port>                  the scripts of sources (Lua), against testserver.py (with --sources)\n"
           "  index <dir>                     checks the index of a directory of sources (index.yaml)\n"
           "--record writes the responses of the sites into the directory, --replay answers the requests from\n"
           "there. The cache is a temporary directory unless --cache is given. The sources are the C++ ones\n"
           "(the reference), --sources adds the scripts of a directory of sources (sources/, they replace\n"
           "the C++ ones of the same name).\n";
    return 2;
}

// The picks: the most popular manga and two others further down the list.
static int pick(MangaSourceBase *source)
{
    MangaList list;
    int failures = captureList(source, &list);
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

static int download(NetworkManager &nm, MangaRepository &repository, MangaSourceBase *source, const QString &title,
                    const QString &url, int count)
{
    auto res = repository.loadInfo(source->name, url, title, false).wait();
    if (!res.has_value())
    {
        out << "info: ERROR " << res.error().message << "\n";
        return 1;
    }
    auto info = res.value();
    count = qMin(count, int(info->chapters.size()));

    MangaChapterDownloadManager manager(&nm, &repository);
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
    // the log goes to stderr (the output/*.log of run.sh), not to the journal
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QApplication app(argc, argv);

    QStringList args = app.arguments().mid(1);
    QString cacheDir, fixturesDir, sourcesDir;
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
        else if (option == "--sources")
            sourcesDir = QDir(value).absolutePath();
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
    CONF.sourcesDir = sourcesDir;
    QDir().mkpath(CONF.mangaListDir);

    if (!Fixtures::install(mode, fixturesDir))
        return 2;

    const auto command = args.takeFirst();
    auto arg = [&](int i) { return args.value(i); };

    int failures;
    if (command == "flow" && args.size() == 3)
    {
        failures = flowTests(arg(0), arg(1), arg(2));
    }
    else if (command == "listflow" && args.size() == 1)
    {
        failures = listFlowTests(arg(0));
    }
    else if (command == "index" && args.size() == 1)
    {
        NetworkManager nm;
        failures = checkIndex(&nm, QDir(arg(0)).absolutePath());
    }
    else if (command == "jobs" || command == "storage" || command == "scripts")
    {
        if (args.isEmpty())
            return usage();
        auto server = "http://127.0.0.1:" + arg(0);
        if (command == "storage")
        {
            failures = storageTests(server);
        }
        else if (command == "scripts")
        {
            failures = scriptTests(server, sourcesDir);
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
        QStringList errors;
        SourceRegistry sources(&settings, harnessSources(&nm, sourcesDir, &errors));
        if (!errors.isEmpty())
            return 2;
        MangaRepository repository(&nm, &sources);
        auto source = sources.source(arg(0)).get();
        if (!source)
        {
            out << "unknown source " << arg(0) << "\n";
            return 2;
        }

        if (command == "capture" && args.size() == 2)
            failures = captureMangas(repository, source, arg(1));
        else if (command == "bench" && args.size() >= 2)
            failures = benchReading(repository, source, arg(1), args.size() > 2 ? arg(2).toInt() : 10,
                                    args.size() > 3 ? arg(3).toInt() : 3);
        else if (command == "list")
            failures = captureList(source);
        else if (command == "pick")
            failures = pick(source);
        else if (command == "download" && args.size() >= 3)
            failures = download(nm, repository, source, arg(1), arg(2), args.size() > 3 ? arg(3).toInt() : 2);

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
