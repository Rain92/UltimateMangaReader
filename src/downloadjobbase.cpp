#include "downloadjobbase.h"

#include <memory>
#include <utility>

static QAtomicInt activeReplyCount;

DownloadJobBase::DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url,
                                 const QList<std::tuple<const char *, const char *>> &customHeaders)
    : networkManager(networkManager),
      reply(),
      managerGuard(networkManager),
      customHeaders(customHeaders),
      retryHold(),
      redirects(0),
      url(url),
      originalUrl(url),
      isCompleted(false),
      errorString()
{
}

DownloadJobBase::~DownloadJobBase()
{
    // the QNetworkAccessManager deletes its replies when it is destroyed first
    if (!managerGuard)
        (void)reply.release();
}

bool DownloadJobBase::hasFailed() const
{
    // an await() that timed out sets errorString while the download is still running
    return !isCompleted && errorString != "" && (!reply || reply->isFinished());
}

QNetworkRequest DownloadJobBase::createRequest() const
{
    return createRequest(url);
}

QNetworkRequest DownloadJobBase::createRequest(const QString &url) const
{
    QNetworkRequest request(url);

    // some hosts reject Qt's default user agent
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/130.0.0.0 Safari/537.36");

    for (const auto &[name, value] : std::as_const(customHeaders))
        request.setRawHeader(name, value);

    // Qt 6 rejects responses that decompress to more than 10 MiB at a ratio above 40 as archive bombs, the
    // sitemaps of the manga lists exceed that (Comick: 300 KB -> 16 MB)
    request.setDecompressedSafetyCheckThreshold(64 * 1024 * 1024);

    return request;
}

bool DownloadJobBase::followRedirect()
{
    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!redirect.isValid())
        return false;

    // relative ones are relative to the request
    redirect = reply->url().resolved(redirect);
    if (redirect == reply->url() || ++redirects > 5)
        return false;

    url = redirect.toString();
    // not right away: this runs in a signal of the reply that restart() deletes
    QTimer::singleShot(0, this, [this]() { restart(); });
    return true;
}

void DownloadJobBase::trackReply(QNetworkReply *reply)
{
    if (!reply)
        return;

    activeReplyCount.ref();

    // finished isn't emitted for replies deleted while running (e.g. on restart), destroyed is
    auto counted = std::make_shared<bool>(true);
    auto release = [counted]()
    {
        if (std::exchange(*counted, false))
            activeReplyCount.deref();
    };
    QObject::connect(reply, &QNetworkReply::finished, reply, release);
    QObject::connect(reply, &QObject::destroyed, release);
}

int DownloadJobBase::activeReplies()
{
    return activeReplyCount.loadRelaxed();
}

std::shared_ptr<void> DownloadJobBase::holdActivity()
{
    activeReplyCount.ref();
    return std::shared_ptr<void>(nullptr, [](void *) { activeReplyCount.deref(); });
}

void DownloadJobBase::onSslErrors(const QList<QSslError> &errors)
{
    for (const QSslError &ssle : errors)
        qDebug() << "SSL Error" << ssle.errorString();

    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply)
        reply->ignoreSslErrors();
}

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda)
{
    if (job->isCompleted)
    {
        lambda();
        return;
    }
    // a job that failed already doesn't signal anymore
    if (job->hasFailed())
        return;

    // the connections keep the job, until the context is deleted
    auto context = new QObject();
    auto release = [context, job]()
    {
        QObject::disconnect(job.get(), nullptr, context, nullptr);
        context->deleteLater();
    };
    QObject::connect(job.get(), &DownloadJobBase::completed, context,
                     [release, lambda]()
                     {
                         release();
                         lambda();
                     });
    QObject::connect(job.get(), &DownloadJobBase::downloadError, context, release);
}
