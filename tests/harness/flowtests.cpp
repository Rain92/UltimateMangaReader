// What the app does with a manga, through the core like the gui: open it, turn its pages into the next
// chapter, open it again (its info updates in the background), make it a favorite and update the favorites,
// download a chapter. Against the site, not compared with golden output: it checks that each step works.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>

#include "harness.h"
#include "ultimatemangareadercore.h"

// processes events until done() or the timeout
static bool waitUntil(const std::function<bool()>& done, int timeout)
{
    QElapsedTimer t;
    t.start();
    while (!done() && t.elapsed() < timeout)
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
    return done();
}

int flowTests(const QString& sourceName, const QString& title, const QString& url)
{
    int failures = 0;
    auto check = [&](bool ok, const QString& what)
    {
        out << (ok ? "ok   " : "FAIL ") << what << "\n";
        out.flush();
        failures += !ok;
    };

    UltimateMangaReaderCore core;
    core.networkManager->setDownloadSettings(QSize(1404, 1872), &core.settings);
    core.networkManager->connected = true;

    QStringList errors;
    auto logError = [&](const QString& e)
    {
        errors << e;
        out << "  error: " << e << "\n";
    };
    QObject::connect(&core, &UltimateMangaReaderCore::error, logError);
    QObject::connect(core.mangaController, &MangaController::error, logError);
    QObject::connect(core.favoritesManager, &FavoritesManager::error, logError);
    QObject::connect(core.mangaChapterDownloadManager, &MangaChapterDownloadManager::error, logError);

    auto source = core.sources->source(sourceName);
    if (!source)
    {
        out << "unknown source " << sourceName << "\n";
        return 1;
    }
    core.sources->setCurrent(source);

    QString shown;
    QObject::connect(core.mangaController, &MangaController::currentImageChanged,
                     [&](const QString& path) { shown = path; });
    auto pageShown = [&]()
    { return waitUntil([&]() { return !shown.isEmpty(); }, 30000) && QFile::exists(shown); };

    // --- open
    QElapsedTimer t;
    t.start();
    core.setCurrentManga(url, title);
    auto info = core.mangaController->currentManga;
    check(info && info->url == url && info->chapters.size() > 1,
          QString("opened: %1 chapters (%2 ms)").arg(info ? info->chapters.size() : 0).arg(t.elapsed()));
    if (!info || info->chapters.size() < 2)
        return failures;
    check(pageShown(), "first page shown");

    // --- the pages, into the next chapter
    int chapter = core.mangaController->currentIndex.chapter;
    int pages = info->chapters[chapter].pages.size();
    shown.clear();
    core.mangaController->setCurrentIndex({chapter, pages - 1});
    check(pageShown(), QString("last page of chapter %1 shown (%2 pages)").arg(chapter).arg(pages));
    bool forward = chapter + 1 < info->chapters.size();
    shown.clear();
    t.restart();
    core.mangaController->advanceMangaPage(forward ? Forward : Backward);
    bool turned = core.mangaController->currentIndex.chapter == chapter + (forward ? 1 : -1) && pageShown();
    check(turned,
          QString("turned into the %1 chapter (%2 ms)").arg(forward ? "next" : "previous").arg(t.elapsed()));
    for (int i = 0; i < 2; i++)
    {
        shown.clear();
        core.mangaController->advanceMangaPage(Forward);
        check(pageShown(), QString("page %1 shown").arg(core.mangaController->currentIndex.page));
    }

    // the preloads of the neighbours
    int nextPage = core.mangaController->currentIndex.page + 1;
    auto nextPath = info->pagePath(core.mangaController->currentIndex.chapter, nextPage);
    check(nextPage >= info->chapters[core.mangaController->currentIndex.chapter].pages.size() ||
              waitUntil([&]() { return QFile::exists(nextPath); }, 30000),
          "next page preloaded");

    // --- opened again: the cached info, updated in the background
    bool updated = false;
    auto c = QObject::connect(info.get(), &MangaInfo::updatedSignal, [&]() { updated = true; });
    t.restart();
    core.setCurrentManga(url, title);
    check(core.mangaController->currentManga == info && t.elapsed() < 1000, "opened again from the cache");
    check(waitUntil([&]() { return updated; }, 30000), "info updated in the background");
    QObject::disconnect(c);

    // --- favorites
    core.favoritesManager->toggleFavorite(info);
    core.favoritesManager->loadInfos();
    updated = false;
    c = QObject::connect(info.get(), &MangaInfo::updatedSignal, [&]() { updated = true; });
    core.favoritesManager->updateInfos();
    check(core.favoritesManager->isFavorite(info) && waitUntil([&]() { return updated; }, 30000),
          "favorite updated");
    QObject::disconnect(c);

    // --- a chapter download
    int target = (core.mangaController->currentIndex.chapter + 1) % info->chapters.size();
    int failed = -1;
    QObject::connect(core.mangaChapterDownloadManager, &MangaChapterDownloadManager::downloadCompleted,
                     [&](int f) { failed = f; });
    t.restart();
    core.mangaChapterDownloadManager->downloadMangaChapters(info, target, target);
    waitUntil([&]() { return failed >= 0; }, 120000);
    int have = 0, n = info->chapters[target].pages.size();
    for (int p = 0; p < n; p++)
        have += QFile::exists(info->pagePath(target, p));
    check(
        failed == 0 && n > 0 && have == n,
        QString("chapter %1 downloaded: %2/%3 pages (%4 ms)").arg(target).arg(have).arg(n).arg(t.elapsed()));

    // the cover came with the info
    check(waitUntil([&]() { return QFile::exists(info->coverPath); }, 20000), "cover downloaded");

    check(errors.isEmpty(), "no errors");
    waitUntil([]() { return DownloadJobBase::activeReplies() == 0; }, 30000);
    return failures;
}

// The update of the manga list through the core like the update dialog: an update, a cancelled one and
// another one.
int listFlowTests(const QString& sourceName)
{
    int failures = 0;
    auto check = [&](bool ok, const QString& what)
    {
        out << (ok ? "ok   " : "FAIL ") << what << "\n";
        out.flush();
        failures += !ok;
    };

    UltimateMangaReaderCore core;
    core.networkManager->setDownloadSettings(QSize(1404, 1872), &core.settings);
    core.networkManager->connected = true;
    if (!core.sources->source(sourceName))
    {
        out << "unknown source " << sourceName << "\n";
        return 1;
    }

    int changed = 0;
    QObject::connect(core.mangaListUpdater, &MangaListUpdater::mangaListChanged,
                     [&](MangaSourceBase* source) { changed += source->name == sourceName; });

    // like the dialog: a token with the sources, the update ends with finished or an error
    struct Outcome
    {
        bool finished = false;
        QString message, error;
    };
    auto update = [&](int cancelAfter) -> Outcome
    {
        QSharedPointer<UpdateProgressToken> token(new UpdateProgressToken());
        token->sourcesProgress[sourceName] = 0;
        Outcome outcome;
        bool ended = false;
        QObject::connect(token.get(), &UpdateProgressToken::updateFinished,
                         [&](const QString& message) { outcome = {true, message, {}}, ended = true; });
        QObject::connect(token.get(), &UpdateProgressToken::updateError,
                         [&](const QString& error) { outcome.error = error, ended = true; });
        core.mangaListUpdater->updateMangaLists(token);

        if (cancelAfter > 0)
        {
            waitUntil([&]() { return ended; }, cancelAfter);
            // like closing the dialog: nobody listens to the token, the updater stops in the background
            token->canceled = true;
            QObject::disconnect(token.get(), nullptr, nullptr, nullptr);
        }
        else
        {
            // MangaDex takes 10+ minutes (its api throttles)
            waitUntil([&]() { return ended; }, 30 * 60000);
        }
        return outcome;
    };

    QElapsedTimer t;
    t.start();
    auto first = update(0);
    auto& list = core.mangaLists.list(sourceName);
    int size = list.size;
    check(first.finished && first.error.isEmpty() && size > 0 && changed == 1,
          QString("updated: %1 titles (%2 s)%3")
              .arg(size)
              .arg(t.elapsed() / 1000)
              .arg(first.message.isEmpty() ? "" : ", " + first.message));

    // read again from its file
    core.mangaLists.reload(sourceName);
    check(core.mangaLists.list(sourceName).size == size, "saved");

    // a cancel keeps the list, the next update can start once the cancelled one stopped
    changed = 0;
    auto cancelled = update(300);
    check(!cancelled.finished && cancelled.error.isEmpty(), "cancelled without a message");
    // an update without sources only says whether the cancelled one still runs
    auto stillRunning = [&]()
    {
        QSharedPointer<UpdateProgressToken> probe(new UpdateProgressToken());
        bool busy = false;
        QObject::connect(probe.get(), &UpdateProgressToken::updateError, [&]() { busy = true; });
        core.mangaListUpdater->updateMangaLists(probe);
        return busy;
    };
    t.restart();
    bool stopped = waitUntil([&]() { return !stillRunning(); }, 60000);
    check(stopped, QString("the cancelled update stopped (%1 ms)").arg(t.elapsed()));
    check(changed == 0 && core.mangaLists.list(sourceName).size == size, "the list is unchanged");

    t.restart();
    auto again = update(0);
    check(again.finished && again.error.isEmpty() && core.mangaLists.list(sourceName).size > 0,
          QString("updated again: %1 titles (%2 s)")
              .arg(core.mangaLists.list(sourceName).size)
              .arg(t.elapsed() / 1000));

    QElapsedTimer drain;
    drain.start();
    while (DownloadJobBase::activeReplies() > 0 && drain.elapsed() < 30000)
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
    return failures;
}
