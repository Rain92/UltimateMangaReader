#include "downloadjobbase.h"

#include <memory>
#include <utility>

static QAtomicInt activeReplyCount;

const QByteArray DownloadJobBase::defaultUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

DownloadJobBase::DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url,
                                 const HttpHeaders &headers)
    : url(url),
      originalUrl(url),
      isCompleted(false),
      errorString(),
      httpStatus(0),
      networkManager(networkManager),
      reply(),
      managerGuard(networkManager),
      headers(headers),
      retryHold(),
      failed(false),
      run(0)
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
    return failed;
}

bool DownloadJobBase::isFinished() const
{
    return isCompleted || failed;
}

QNetworkRequest DownloadJobBase::createRequest(const QString &url, const HttpHeaders &headers)
{
    QNetworkRequest request(url);

    request.setRawHeader("User-Agent", defaultUserAgent);
    for (const auto &header : headers)
        request.setRawHeader(header.name, header.value);

    // Qt 6 rejects responses that decompress to more than 10 MiB at a ratio above 40 as archive bombs, the
    // sitemaps of the manga lists exceed that (Comick: 300 KB -> 16 MB)
    request.setDecompressedSafetyCheckThreshold(64 * 1024 * 1024);

    return request;
}

QNetworkReply *DownloadJobBase::get(const QString &url)
{
    auto reply = networkManager->get(createRequest(url, headers));
    prepareReply(reply);
    return reply;
}

void DownloadJobBase::resetState()
{
    run++;
    url = originalUrl;
    isCompleted = false;
    failed = false;
    errorString = "";
    httpStatus = 0;
    retryHold.reset();
}

void DownloadJobBase::finishCompleted()
{
    if (isFinished())
        return;

    retryHold.reset();
    isCompleted = true;
    QMetaObject::invokeMethod(
        this,
        [this, finishedRun = run]()
        {
            if (run == finishedRun)
                emit completed();
        },
        Qt::QueuedConnection);
}

void DownloadJobBase::finishFailed(const QString &error)
{
    if (isFinished())
        return;

    retryHold.reset();
    failed = true;
    errorString = error;
    qDebug() << "Download failed:" << originalUrl << error;
    QMetaObject::invokeMethod(
        this,
        [this, finishedRun = run]()
        {
            if (run == finishedRun)
                emit downloadError();
        },
        Qt::QueuedConnection);
}

void DownloadJobBase::abortWith(const QString &error)
{
    finishFailed(error);
    if (reply)
    {
        // only the job's own handlers: the abort finishes the reply, which releases its activity count
        reply->disconnect(this);
        reply->abort();
    }
}

QString DownloadJobBase::replyErrorString(QNetworkReply *reply)
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return status > 0 ? QString("Download error %1: %2").arg(status).arg(reply->errorString())
                      : QString("Download error: %1").arg(reply->errorString());
}

bool DownloadJobBase::isTransientError(QNetworkReply *reply)
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 408 || status == 429 || status >= 500)
        return true;
    if (status > 0)
        return false;

    // no answer: anything but errors of the request itself
    switch (reply->error())
    {
        case QNetworkReply::ProtocolUnknownError:
        case QNetworkReply::ProtocolInvalidOperationError:
        case QNetworkReply::InsecureRedirectError:
        case QNetworkReply::TooManyRedirectsError:
            return false;
        default:
            return true;
    }
}

void DownloadJobBase::prepareReply(QNetworkReply *reply)
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

    QObject::connect(reply, &QNetworkReply::sslErrors, reply,
                     [reply](const QList<QSslError> &errors)
                     {
                         for (const QSslError &error : errors)
                             qDebug() << "SSL Error" << reply->url().host() << error.errorString();
                         reply->ignoreSslErrors();
                     });
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

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda)
{
    if (job->isCompleted)
    {
        lambda();
        return;
    }
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
