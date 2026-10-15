// The contract of the download jobs, against tests/harness/testserver.py.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include "downloadqueue.h"
#include "harness.h"
#include "networkmanager.h"
#include "staticsettings.h"
#include "task.h"
#include "utils.h"

// coroutines for the tests of Task

static Task<Result<QString>> fetchText(NetworkManager *nm, QString url)
{
    DownloadRequest request;
    request.url = url;
    request.timeout = 3000;
    auto job = nm->download(request);
    if (!co_await job)
        co_return Err(job->error);
    co_return job->text();
}

// two downloads at the same time, the second one is awaited after the first
static Task<qsizetype> fetchTwo(NetworkManager *nm, QString url1, QString url2)
{
    auto a = fetchText(nm, url1);
    auto b = fetchText(nm, url2);
    auto textA = co_await a;
    auto textB = co_await b;
    co_return (textA ? textA->size() : -100) + (textB ? textB->size() : -100);
}

static Task<> countLater(int *counter, int ms)
{
    co_await delay(ms);
    (*counter)++;
}

static Task<bool> waitAtMost(NetworkManager *nm, QString url, int maxWait)
{
    DownloadRequest request;
    request.url = url;
    auto job = nm->download(request);
    bool completed = co_await waitFor(job, maxWait);
    co_return completed || job->isFinished();
}

static Task<bool> firstOfTwo(NetworkManager *nm, QString slowUrl, QString fastUrl)
{
    DownloadRequest slow, fast;
    slow.url = slowUrl;
    fast.url = fastUrl;
    auto slowJob = nm->download(slow);
    auto fastJob = nm->download(fast);
    co_await anyFinished({slowJob.get(), fastJob.get()}, 0);
    co_return fastJob->isCompleted && !slowJob->isFinished();
}

static Task<bool> awaitQueue(NetworkManager *nm, QStringList urls)
{
    DownloadQueue queue(nm, urls, 2, nullptr, false);
    queue.start();
    co_return co_await queue;
}

int jobTests(NetworkManager &nm, const QString &base)
{
    int failures = 0;
    auto check = [&](const char *what, bool ok)
    {
        out << (ok ? "PASS " : "FAIL ") << what << "\n";
        failures += !ok;
    };
    auto request = [&](const QString &path, int timeout = 3000)
    {
        DownloadRequest request;
        request.url = base + path;
        request.timeout = timeout;
        return request;
    };

    {
        auto job = nm.download(request("/delay/3", 500));
        QElapsedTimer t;
        t.start();
        bool ok = job->await();
        check("the timeout ends the download", !ok && t.elapsed() < 1500);
        check("timeout error", job->error.code == Error::Timeout && job->error.message == "Download timeout.");
        check("timed out job is finished and aborted", job->hasFailed() && DownloadJobBase::activeReplies() == 0);
    }
    {
        auto job = nm.download(request("/delay/1", 3000));
        bool ok = job->await(200);
        check("await with a limit doesn't stop the download", !ok && !job->isFinished());
        check("the download goes on", job->await() && job->text() == "late");
    }
    {
        auto job = nm.download(request("/status/404"));
        bool ok = job->await();
        check("404 fails without retry", !ok && job->httpStatus == 404 && job->error.code == Error::Http &&
                                             job->error.httpStatus == 404 && job->error.message.contains("404"));
        check("the body of an error response is kept", job->text() == "status");
    }
    {
        auto job = nm.download(request("/flaky/1"));
        bool ok = job->await();
        check("503 is retried", ok && job->text() == "ok after retry");
    }
    {
        auto plain = nm.download(request("/flaky400/1"));
        auto r = request("/flaky400/2");
        r.retryStatuses = {400};
        auto retried = nm.download(r);
        check("400 isn't retried by default", !plain->await() && plain->httpStatus == 400);
        check("statuses of the request are retried", retried->await() && retried->text() == "ok after 400");
    }
    {
        auto job = nm.download(request("/retryafter/1/a", 4000));
        QElapsedTimer t;
        t.start();
        bool ok = job->await();
        check("Retry-After is waited for", ok && t.elapsed() >= 950 && job->text() == "ok after retry-after");

        auto late = nm.download(request("/retryafter/2/b", 1000));
        t.restart();
        check("a Retry-After past the timeout fails right away",
              !late->await() && late->httpStatus == 429 && t.elapsed() < 500);
    }
    {
        auto r = request("/status/503", 1200);
        auto job = nm.download(r);
        QElapsedTimer t;
        t.start();
        bool ok = job->await();
        check("retries end in time", !ok && job->httpStatus == 503 && t.elapsed() >= 450 && t.elapsed() < 1300);
        check("no activity left after the retries", DownloadJobBase::activeReplies() == 0);
    }
    {
        auto job = nm.download(request("/redirect"));
        bool ok = job->await();
        check("redirect followed, final url", ok && job->text() == "final" && job->url.endsWith("/final") &&
                                                  job->request.url.endsWith("/redirect"));
    }
    {
        auto r = request("/headers");
        r.headers = {{"Referer", "https://example.org/"}};
        auto job = nm.download(r);
        bool ok = job->await();
        check("request headers sent", ok && job->text().contains("Referer: https://example.org/") &&
                                          job->text().contains("Chrome/"));
    }
    {
        auto job = nm.download(request("/image/10x10"));
        bool ok = job->await();
        check("response headers, content type", ok && job->contentType() == "image/png" &&
                                                    job->responseHeader("content-type") == "image/png" &&
                                                    !job->responseHeaders.isEmpty());

        auto r = request("/file/head");
        r.headOnly = true;
        auto head = nm.download(r);
        check("HEAD: the headers only", head->await() && head->httpStatus == 200 && head->buffer.isEmpty() &&
                                            head->responseHeader("Content-Length") == "10000");
    }
    {
        auto job = nm.download(request("/delay/2"));
        QTimer::singleShot(100, job.get(), [job]() { job->cancel(); });
        QElapsedTimer t;
        t.start();
        bool ok = job->await();
        check("cancel ends the download", !ok && t.elapsed() < 1000 && job->error.isCancelled());
        QCoreApplication::processEvents();
        check("a cancelled download leaves no activity", DownloadJobBase::activeReplies() == 0);
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
        check("unwritable file fails from the event loop",
              signalled && !synchronous && job->error.code == Error::Storage && job->error.message.startsWith("Can't create file"));
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
        a.clear();
        a2.clear();
        c.clear();
        b.clear();
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
        DownloadQueue queue2(
            &nm, {base + "/delay/0.2", base + "/delay/0.2", base + "/delay/0.2", base + "/delay/0.2"}, 1,
            [&](QSharedPointer<DownloadBufferJob>)
            {
                if (++done == 1)
                    cancel = true;
            },
            false);
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

        auto job = nm.download(request("/delay/1"));
        int whileWaiting = -1;
        QTimer::singleShot(300, [&whileWaiting]() { whileWaiting = DownloadJobBase::activeReplies(); });
        job->await();
        check("await holds activity besides its reply", whileWaiting == 2 && DownloadJobBase::activeReplies() == 0);
    }
    {
        DownloadRequest r = request("/file/start");
        r.maxBytes = 4096;
        auto job = nm.download(r);
        check("maxBytes: the start of the body", job->await() && job->buffer.size() == 4096);
    }

    // --- Task
    {
        auto text = fetchText(&nm, base + "/final").wait();
        check("a task waits for its download, wait() gets the result", text && *text == "final");

        auto failed = fetchText(&nm, base + "/status/404").wait();
        check("a failed download is the error of the task", !failed && failed.error().code == Error::Http &&
                                                                 failed.error().httpStatus == 404);

        QElapsedTimer t;
        t.start();
        auto sum = fetchTwo(&nm, base + "/delay/0.3", base + "/delay/0.3").wait();
        check("tasks run at the same time", sum == 8 && t.elapsed() < 550);

        int counter = 0;
        countLater(&counter, 50);
        t.restart();
        while (counter == 0 && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
        check("a task finishes without being kept", counter == 1);

        auto task = fetchText(&nm, base + "/delay/0.1");
        QString late, now;
        task.then([&](const Result<QString> &r) { late = r.value_or("error"); });
        check("then: not before it finished", late.isEmpty() && !task.isFinished());
        (void)task.wait();
        task.then([&](const Result<QString> &r) { now = r.value_or("error"); });
        check("then: when it finished, right away for a finished one", late == "late" && now == "late");

        t.restart();
        bool done = waitAtMost(&nm, base + "/delay/1", 200).wait();
        check("waitFor goes on after its limit", !done && t.elapsed() < 600);

        check("anyFinished goes on with the first one",
              firstOfTwo(&nm, base + "/delay/1", base + "/final").wait());

        check("co_await of a queue", awaitQueue(&nm, {base + "/final", base + "/delay/0.2"}).wait() &&
                                         !awaitQueue(&nm, {base + "/final", base + "/status/404"}).wait());

        int during = -1;
        QTimer::singleShot(100, [&during]() { during = DownloadJobBase::activeReplies(); });
        (void)fetchText(&nm, base + "/delay/0.3").wait();
        check("waiting for a task holds activity", during >= 2);

        // the downloads that the tests left behind
        t.restart();
        while (DownloadJobBase::activeReplies() > 0 && t.elapsed() < 3000)
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
    }

    check("no active replies at the end", DownloadJobBase::activeReplies() == 0);
    return failures;
}

