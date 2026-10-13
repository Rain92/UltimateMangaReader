#include "downloadfilejob.h"

#include "utils.h"

DownloadFileJob::DownloadFileJob(QNetworkAccessManager *networkManager, const QString &url,
                                 const QString &localFilePath,
                                 const QList<std::tuple<const char *, const char *>> &customHeaders)
    : DownloadJobBase(networkManager, url, customHeaders), retries(0), filepath(localFilePath)
{
}

void DownloadFileJob::start()
{
    QString dirname = QFileInfo(filepath).path();
    QDir().mkpath(dirname);

    file.setFileName(filepath + ".part");

    if (QFile::exists(filepath))
    {
        isCompleted = true;
    }
    else
    {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QNetworkRequest request = createRequest();
            // abort stalled downloads, otherwise a DownloadQueue waits forever
            request.setTransferTimeout(20000);

            reply.reset(networkManager->get(request));
            //            reply->setParent(nullptr);
            trackReply(reply.get());

            QObject::connect(reply.get(), &QNetworkReply::readyRead, this,
                             &DownloadFileJob::downloadFileReadyRead);
            QObject::connect(reply.get(), &QNetworkReply::finished, this,
                             &DownloadFileJob::downloadFileFinished);
            QObject::connect(reply.get(), &QNetworkReply::sslErrors, this, &DownloadJobBase::onSslErrors);
        }
        else
        {
            errorString = "Can't create file.";
            emit downloadError();
        }
    }
}

void DownloadFileJob::restart()
{
    isCompleted = false;
    errorString = "";
    retryHold.reset();
    reply.reset();
    start();

    // the file can exist by now (another job for the same path), whoever waits for the job needs the signal
    if (isCompleted)
        QTimer::singleShot(0, this, [this]() { emit completed(); });
}

void DownloadFileJob::downloadFileReadyRead()
{
    file.write(reply->readAll());
}

void DownloadFileJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    if (followRedirect())
        return;

    if (reply->error() != QNetworkReply::NoError)
    {
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        isCompleted = true;

        file.rename(filepath);

        emit completed();
    }
}

void DownloadFileJob::onError(QNetworkReply::NetworkError)
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    file.remove();

    // overloaded servers, rate limits and connection problems often go away after a moment. Only the last
    // attempt counts as failed, until then the job is handed out to whoever asks for the same file.
    const int maxRetries = 2;
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool transient = status == 0 || status == 408 || status == 429 || status >= 500;
    if (transient && retries < maxRetries && errorString == "")
    {
        int delayMs = 1000 << retries++;
        qDebug() << "Retrying in" << delayMs << "ms:" << url << reply->errorString();
        retryHold = holdActivity();
        // not right away: this runs in a signal of the reply that restart() deletes
        QTimer::singleShot(delayMs, this, &DownloadFileJob::restart);
        return;
    }

    if (errorString == "")
        errorString = "Download error " +
                      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString() + ": " +
                      reply->errorString();

    qDebug() << "Error string:" << errorString;
    emit downloadError();
}

bool DownloadFileJob::await(int timeout)
{
    if (isCompleted)
        return true;

    awaitSignal(this, {SIGNAL(completed()), SIGNAL(downloadError())}, timeout);

    if (isCompleted)
        return true;

    if (errorString == "")
        errorString = "Download timeout.";

    return false;
}
