#pragma once

#include <QtNetwork>

#include <functional>
#include <memory>

#include "httpheader.h"
#include "result.h"

// What to download and how hard to try.
struct DownloadRequest
{
    QString url;
    HttpHeaders headers;
    // HEAD: only the status and the headers of the response
    bool headOnly = false;
    // the download with all its attempts fails after this (ms), 0: no limit
    int timeout = 6000;
    // fail on TLS errors (by default they are ignored, see DownloadJobBase::prepareReply())
    bool verifyTls = false;
    // an attempt that transfers nothing for this long fails (ms), 0: no limit
    int transferTimeout = 0;
    // Only the start of the body (of a successful response): the download completes when it has this many
    // bytes, 0: all of it.
    qint64 maxBytes = 0;

    // Attempts after errors that may go away (no answer, 408, 429, 5xx and the retryStatuses). The pause before
    // one doubles from retryDelay up to maxRetryDelay, unless the server asks for another one (Retry-After).
    int maxRetries = 3;
    int retryDelay = 500;
    int maxRetryDelay = 4000;
    QList<int> retryStatuses;
};

// How the downloads of a source's images are retried (see DownloadRequest; the defaults are the ones of
// DownloadFileJob::defaultRequest()), e.g. longer for a CDN that answers 429 while it loads an image into its cache.
struct ImageRetries
{
    int maxRetries = 2;
    int retryDelay = 1000;
    int maxRetryDelay = 4000;
};

// A download. Every run (start() or restart()) finishes once, after its retries, with completed() or
// downloadError(). Both are emitted from the event loop, never from within a call to the job, and only when the
// job's network requests are done. The response (url, httpStatus, responseHeaders, error and the data of the
// subclass) is set when it finishes, before the signal.
class DownloadJobBase : public QObject
{
    Q_OBJECT

public:
    DownloadJobBase(QNetworkAccessManager *networkManager, const DownloadRequest &request);
    virtual ~DownloadJobBase();

    const DownloadRequest request;

    // the url that answered (after redirects)
    QString url;
    bool isCompleted;
    // why it failed (Timeout, Cancelled, Http with the status, Network, Storage for a file that can't be written)
    Error error;
    // of the last response, also of failed ones (0 without a response, e.g. no connection)
    int httpStatus;
    HttpHeaders responseHeaders;
    // The first header of the response with that name (case insensitive), empty if there is none.
    QByteArray responseHeader(const QByteArray &name) const;
    QByteArray contentType() const { return responseHeader("Content-Type"); }

    bool hasFailed() const;
    bool isFinished() const;

    void start();
    // Starts over (e.g. after it failed).
    void restart();
    // Stops the download, it fails.
    void cancel();

    // Waits until the download finished, at most maxWait ms (no limit if <= 0, the download's timeout ends it).
    // The download goes on after maxWait (others may wait for it too). Returns whether it completed.
    bool await(int maxWait = 0);

    // Counts reply as active until it finished or got destroyed, and ignores its TLS errors unless verifyTls
    // (the sources' hosts often have broken certificate chains, the device's clock can be wrong).
    static void prepareReply(QNetworkReply *reply, bool verifyTls = false);
    // Number of prepared network replies that are still running.
    static int activeReplies();
    // Counts as one more active reply until the returned handle is released (e.g. while waiting to retry: the
    // device mustn't go to standby then, it would stop the timer). The blocking waits for downloads hold it.
    static std::shared_ptr<void> holdActivity();
    // Waits ms in an event loop (e.g. between throttled requests), holding the activity.
    static void waitMs(qint64 ms);

    // The User-Agent that requests get by default (some hosts reject Qt's).
    static const QByteArray defaultUserAgent;
    static QNetworkRequest createRequest(const QString &url, const HttpHeaders &headers);

signals:
    void completed();
    void downloadError();

protected:
    QNetworkAccessManager *networkManager;
    std::unique_ptr<QNetworkReply> reply;
    QPointer<QNetworkAccessManager> managerGuard;

    // Sends the request of an attempt (the first one and the retries).
    virtual void startAttempt() = 0;
    // Clears the response for another run.
    virtual void resetState();

    // A prepared request of the job to url (GET, or HEAD for a headOnly request).
    QNetworkReply *send(const QString &url);
    // Takes the url, status and headers of the response.
    void takeResponse(QNetworkReply *reply);
    // After an attempt failed: starts another one later, if the error may go away and the retries and the time
    // allow it. Returns whether it will.
    bool retryLater(QNetworkReply *reply);

    // Finish the run. The signal is emitted from the event loop (not for a run that was restarted meanwhile).
    void finishCompleted();
    void finishFailed(const Error &error);
    // Stops the running request and fails with error.
    void abortWith(const Error &error);

    // Http with the status, or Network without an answer: "Download error 404: ..."
    static Error replyError(QNetworkReply *reply);

private:
    bool failed;
    // counts the runs, a signal is only emitted for the current one
    int run;
    int retries;
    QTimer deadlineTimer;
    QTimer retryTimer;
    // held while the job waits to try again, see holdActivity()
    std::shared_ptr<void> retryHold;

    // Errors that may go away by trying again: no answer, a timeout, rate limits, server errors and the
    // request's retryStatuses.
    bool isTransientError(QNetworkReply *reply) const;
    // The pause the server asks for (Retry-After), -1 if it doesn't.
    static qint64 retryAfter(QNetworkReply *reply);
};

// Calls lambda (on the job's thread) when the job completes, the job is kept until it finished.
void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda);
