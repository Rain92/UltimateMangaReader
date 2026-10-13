// The contract of the download jobs, against tests/harness/testserver.py.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include "downloadqueue.h"
#include "harness.h"
#include "networkmanager.h"
#include "staticsettings.h"
#include "utils.h"

int jobTests(NetworkManager &nm, const QString &base)
{
    int failures = 0;
    auto check = [&](const char *what, bool ok)
    {
        out << (ok ? "PASS " : "FAIL ") << what << "\n";
        failures += !ok;
    };

    {
        auto job = nm.downloadAsString(base + "/delay/3");
        QElapsedTimer t;
        t.start();
        bool ok = job->await(500);
        check("await timeout returns false in time", !ok && t.elapsed() < 1500);
        check("timeout error", job->errorString == "Download timeout.");
        check("timed out job is finished and aborted", job->hasFailed() && DownloadJobBase::activeReplies() == 0);
    }
    {
        auto job = nm.downloadAsString(base + "/status/404");
        bool ok = job->await(3000);
        check("404 fails without retry", !ok && job->httpStatus == 404 && job->errorString.contains("404"));
    }
    {
        auto job = nm.downloadAsString(base + "/flaky/1");
        bool ok = job->await(3000);
        check("503 is retried", ok && job->bufferStr == "ok after retry");
    }
    {
        auto job = nm.downloadAsString(base + "/redirect");
        bool ok = job->await(3000);
        check("redirect followed, final url", ok && job->bufferStr == "final" && job->url.endsWith("/final") &&
                                                  job->originalUrl.endsWith("/redirect"));
    }
    {
        auto job = nm.downloadAsString(base + "/headers", 3000, {{"Referer", "https://example.org/"}});
        bool ok = job->await(3000);
        check("request headers sent", ok && job->bufferStr.contains("Referer: https://example.org/") &&
                                          job->bufferStr.contains("Chrome/"));
    }
    {
        // the signals come from the event loop, also for a job that fails right away
        bool signalled = false, synchronous = true;
        auto job = nm.downloadAsFile(base + "/file/a", "/proc/nonexistent/dir/a.jpg");
        QObject::connect(job.get(), &DownloadJobBase::downloadError, [&]() { signalled = true; });
        synchronous = signalled;
        QElapsedTimer t;
        t.start();
        while (!signalled && t.elapsed() < 2000)
            QCoreApplication::processEvents();
        check("unwritable file fails from the event loop", signalled && !synchronous && job->errorString.startsWith("Can't create file"));
    }
    {
        QString dir = CONF.cacheDir + "jobtest/";
        QDir(dir).removeRecursively();
        auto a = nm.downloadAsFile(base + "/file/same", dir + "a.bin");
        auto b = nm.downloadAsFile(base + "/file/same", dir + "b.bin");
        auto a2 = nm.downloadAsFile(base + "/file/same", dir + "a.bin");
        bool ok = a->await(3000) && b->await(3000);
        check("same url, two files: both written", ok && a != b && a == a2 && QFileInfo(dir + "a.bin").size() == 10000 &&
                                                      QFileInfo(dir + "b.bin").size() == 10000 &&
                                                      !QFile::exists(dir + "a.bin.part"));
        auto c = nm.downloadAsFile(base + "/file/same", dir + "a.bin");
        check("a finished job that is still held is handed out", c == a && c->isCompleted);
        a.clear(); a2.clear(); c.clear(); b.clear();
        QCoreApplication::processEvents();

        bool again = false;
        auto d = nm.downloadAsFile(base + "/file/same", dir + "a.bin");
        QObject::connect(d.get(), &DownloadJobBase::completed, [&]() { again = true; });
        awaitSignal(d.get(), 2000, &DownloadJobBase::completed);
        check("new job for an existing file completes (signalled)", d->isCompleted && again);
    }
    {
        DownloadQueue queue(&nm, QStringList(), 2, nullptr, false);
        queue.start();
        QElapsedTimer t;
        t.start();
        bool ok = queue.awaitCompletion();
        check("empty queue doesn't hang", ok && t.elapsed() < 100);

        bool cancel = false;
        int done = 0;
        DownloadQueue queue2(&nm, {base + "/delay/0.2", base + "/delay/0.2", base + "/delay/0.2", base + "/delay/0.2"},
                             1, [&](QSharedPointer<DownloadStringJob>) { if (++done == 1) cancel = true; }, false);
        queue2.setCancellationToken(&cancel);
        queue2.start();
        bool ok2 = queue2.awaitCompletion();
        check("cancelled queue counts one error", !ok2 && queue2.errors == 1 && done == 1);
        check("finished queue doesn't hang", !queue2.awaitCompletion());
    }
    {
        int during = -1;
        QTimer::singleShot(50, [&during]() { during = DownloadJobBase::activeReplies(); });
        DownloadJobBase::waitMs(200);
        check("a pause counts as activity until it ends", during == 1 && DownloadJobBase::activeReplies() == 0);

        auto job = nm.downloadAsString(base + "/delay/1");
        int whileWaiting = -1;
        QTimer::singleShot(300, [&whileWaiting]() { whileWaiting = DownloadJobBase::activeReplies(); });
        job->await(3000);
        check("await holds activity besides its reply", whileWaiting == 2 && DownloadJobBase::activeReplies() == 0);
    }
    check("no active replies at the end", DownloadJobBase::activeReplies() == 0);
    return failures;
}
