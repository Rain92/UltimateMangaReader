#include "downloadjobbase.h"

#include <memory>
#include <utility>

#include "utils.h"

static QAtomicInt activeReplyCount;

const QByteArray DownloadJobBase::defaultUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

DownloadJobBase::DownloadJobBase(QNetworkAccessManager *networkManager, const DownloadRequest &request)
    : request(request),
      url(request.url),
      isCompleted(false),
      error(),
      httpStatus(0),
      responseHeaders(),
      networkManager(networkManager),
      reply(),
      managerGuard(networkManager),
      failed(false),
      run(0),
      retries(0),
      deadlineTimer(),
      retryTimer(),
      retryHold()
{
    deadlineTimer.setSingleShot(true);
    connect(&deadlineTimer, &QTimer::timeout, this,
            [this]() { abortWith({Error::Timeout, "Download timeout.", httpStatus}); });
    retryTimer.setSingleShot(true);
    connect(&retryTimer, &QTimer::timeout, this,
            [this]()
            {
                retryHold.reset();
                if (!isFinished())
                    startAttempt();
            });
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

QByteArray DownloadJobBase::responseHeader(const QByteArray &name) const
{
    for (const auto &header : responseHeaders)
        if (header.name.compare(name, Qt::CaseInsensitive) == 0)
            return header.value;
    return {};
}

void DownloadJobBase::start()
{
    retries = 0;
    if (request.timeout > 0)
        deadlineTimer.start(request.timeout);
    startAttempt();
}

void DownloadJobBase::restart()
{
    resetState();
    start();
}

void DownloadJobBase::cancel()
{
    abortWith({Error::Cancelled, "Download cancelled", 0});
}

bool DownloadJobBase::await(int maxWait)
{
    auto hold = holdActivity();
    if (!isFinished())
        awaitSignal(this, maxWait, &DownloadJobBase::completed, &DownloadJobBase::downloadError);

    return isCompleted;
}

QNetworkReply *DownloadJobBase::send(const QString &url)
{
    auto networkRequest = createRequest(url, request.headers);
    if (request.transferTimeout > 0)
        networkRequest.setTransferTimeout(request.transferTimeout);

    auto reply = request.headOnly ? networkManager->head(networkRequest) : networkManager->get(networkRequest);
    prepareReply(reply, request.verifyTls);
    return reply;
}

void DownloadJobBase::takeResponse(QNetworkReply *reply)
{
    url = reply->url().toString();
    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    responseHeaders.clear();
    const auto headers = reply->headers();
    for (qsizetype i = 0; i < headers.size(); i++)
        responseHeaders.append({headers.nameAt(i).toString().toLatin1(), headers.valueAt(i).toByteArray()});
}

void DownloadJobBase::resetState()
{
    run++;
    url = request.url;
    isCompleted = false;
    failed = false;
    error = {};
    httpStatus = 0;
    responseHeaders.clear();
    deadlineTimer.stop();
    retryTimer.stop();
    retryHold.reset();
    // only the job's own handlers: the abort finishes the reply, which releases its activity count
    if (reply)
    {
        reply->disconnect(this);
        reply->abort();
    }
    reply.reset();
}

bool DownloadJobBase::retryLater(QNetworkReply *reply)
{
    if (isFinished() || retries >= request.maxRetries || !isTransientError(reply))
        return false;

    qint64 delay = retryAfter(reply);
    // a server that wants a break of more than a minute isn't asked again
    if (delay > 60000)
        return false;
    if (delay < 0)
        delay = qMin<qint64>(qint64(request.retryDelay) << retries, request.maxRetryDelay);

    // no time left for another attempt
    if (deadlineTimer.isActive() && deadlineTimer.remainingTime() < delay + 100)
        return false;

    retries++;
    qDebug() << "Retrying in" << delay << "ms:" << request.url << replyError(reply).message;
    // no standby until then, it would stop the timer
    retryHold = holdActivity();
    retryTimer.start(int(delay));
    return true;
}

qint64 DownloadJobBase::retryAfter(QNetworkReply *reply)
{
    auto value = reply->headers().value(QHttpHeaders::WellKnownHeader::RetryAfter).toByteArray().trimmed();
    if (value.isEmpty())
        return -1;

    bool ok = false;
    qint64 seconds = value.toLongLong(&ok);
    if (ok)
        return qMax<qint64>(0, seconds * 1000);

    auto time = QDateTime::fromString(QString::fromLatin1(value), Qt::RFC2822Date);
    return time.isValid() ? qMax<qint64>(0, QDateTime::currentDateTimeUtc().msecsTo(time)) : -1;
}

void DownloadJobBase::finishCompleted()
{
    if (isFinished())
        return;

    deadlineTimer.stop();
    retryTimer.stop();
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

void DownloadJobBase::finishFailed(const Error &error)
{
    if (isFinished())
        return;

    deadlineTimer.stop();
    retryTimer.stop();
    retryHold.reset();
    failed = true;
    this->error = error;
    qDebug() << "Download failed:" << request.url << error.message;
    QMetaObject::invokeMethod(
        this,
        [this, finishedRun = run]()
        {
            if (run == finishedRun)
                emit downloadError();
        },
        Qt::QueuedConnection);
}

void DownloadJobBase::abortWith(const Error &error)
{
    finishFailed(error);
    if (reply)
    {
        // only the job's own handlers: the abort finishes the reply, which releases its activity count
        reply->disconnect(this);
        reply->abort();
    }
}

Error DownloadJobBase::replyError(QNetworkReply *reply)
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status > 0)
        return {Error::Http, QString("Download error %1: %2").arg(status).arg(reply->errorString()), status};

    // the transfer timeout of the request
    auto code = reply->error() == QNetworkReply::OperationCanceledError ? Error::Timeout : Error::Network;
    return {code, QString("Download error: %1").arg(reply->errorString()), 0};
}

bool DownloadJobBase::isTransientError(QNetworkReply *reply) const
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 408 || status == 429 || status >= 500 || request.retryStatuses.contains(status))
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

void DownloadJobBase::prepareReply(QNetworkReply *reply, bool verifyTls)
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
                     [reply, verifyTls](const QList<QSslError> &errors)
                     {
                         for (const QSslError &error : errors)
                             qDebug() << "SSL Error" << reply->url().host() << error.errorString();
                         if (!verifyTls)
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

void DownloadJobBase::waitMs(qint64 ms)
{
    if (ms <= 0)
        return;

    auto hold = holdActivity();
    QEventLoop loop;
    QTimer::singleShot(static_cast<int>(ms), &loop, &QEventLoop::quit);
    loop.exec();
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
