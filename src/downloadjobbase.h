#pragma once

#include <QtNetwork>

#include <functional>
#include <memory>

#include "httpheader.h"

// A download. Every run (start() or restart()) finishes once, with completed() or downloadError(). Both are
// emitted from the event loop, never from within a call to the job, and only when the job's network requests
// are done. The state (isCompleted, hasFailed(), errorString) is set when it finishes, before the signal.
class DownloadJobBase : public QObject
{
    Q_OBJECT

public:
    DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url, const HttpHeaders &headers = {});
    virtual ~DownloadJobBase();

    // the url that answered (after redirects)
    QString url;
    // the requested url
    const QString originalUrl;
    bool isCompleted;
    QString errorString;
    // of the last response, also of failed ones (0 without a response, e.g. no connection)
    int httpStatus;

    bool hasFailed() const;
    bool isFinished() const;

    virtual void start() = 0;
    // Starts over (e.g. after a failure).
    virtual void restart() = 0;

    // Counts reply as active until it finished or got destroyed, and ignores its TLS errors (the sources'
    // hosts often have broken certificate chains, the device's clock can be wrong).
    static void prepareReply(QNetworkReply *reply);
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

    // Errors that may go away by trying again: no answer, a timeout, rate limits and server errors.
    static bool isTransientError(QNetworkReply *reply);

signals:
    void completed();
    void downloadError();

protected:
    QNetworkAccessManager *networkManager;
    std::unique_ptr<QNetworkReply> reply;
    QPointer<QNetworkAccessManager> managerGuard;
    HttpHeaders headers;
    // held while the job waits to try again, see holdActivity()
    std::shared_ptr<void> retryHold;

    // Sends a GET request with the job's headers.
    QNetworkReply *get(const QString &url);

    // Resets the state for another run.
    void resetState();
    // Finish the run. The signal is emitted from the event loop (not for a run that was restarted meanwhile).
    void finishCompleted();
    void finishFailed(const QString &error);
    // Stops the running request and fails with error.
    void abortWith(const QString &error);

    // "Download error 404: ..."
    static QString replyErrorString(QNetworkReply *reply);

private:
    bool failed;
    // counts the runs, a signal is only emitted for the current one
    int run;
};

// Calls lambda (on the job's thread) when the job completes, the job is kept until it finished.
void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda);
