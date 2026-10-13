#ifndef DOWNLOADJOBBASE_H
#define DOWNLOADJOBBASE_H

#include <QTime>
#include <QtNetwork>

#include <functional>
#include <memory>

class DownloadJobBase : public QObject
{
    Q_OBJECT

protected:
    QNetworkAccessManager *networkManager;
    std::unique_ptr<QNetworkReply> reply;
    QPointer<QNetworkAccessManager> managerGuard;
    QList<std::tuple<const char *, const char *>> customHeaders;
    // held while the job waits to try again, see holdActivity()
    std::shared_ptr<void> retryHold;

    QNetworkRequest createRequest() const;
    QNetworkRequest createRequest(const QString &url) const;

    // Qt 5 doesn't follow redirects by itself (and Qt 6 not from https to http): restarts the job with the
    // target of a redirect reply. Returns false if the reply isn't one.
    bool followRedirect();
    int redirects;

signals:
    void completed();
    void downloadError();

public:
    DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url,
                    const QList<std::tuple<const char *, const char *>> &customHeaders = {});
    virtual ~DownloadJobBase();

    QString url;
    QString originalUrl;
    bool isCompleted;
    QString errorString;

    bool hasFailed() const;

    // Counts reply as active until it finished or got destroyed.
    static void trackReply(QNetworkReply *reply);
    // Number of tracked network replies that are still running.
    static int activeReplies();
    // Counts as one more active reply until the returned handle is released (e.g. while waiting to retry: the
    // device mustn't go to standby then, it would stop the timer).
    static std::shared_ptr<void> holdActivity();

    virtual void start() = 0;
    virtual void restart() = 0;
    virtual void onSslErrors(const QList<QSslError> &);
};

// Calls lambda (on the job's thread) when the job completes, the job is kept until then.
void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda);

#endif  // DOWNLOADJOBBASE_H
