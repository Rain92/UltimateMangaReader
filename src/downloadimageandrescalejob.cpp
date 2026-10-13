#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, ImageProcessor *imageProcessor, const QString &url,
    const QString &path, QSize screenSize, Settings *settings,
    const QList<std::tuple<const char *, const char *>> &customHeaders, const EncryptionDescriptor &encryption,
    const QString &appendUrl)
    : DownloadFileJob(networkManager, url, path, customHeaders),
      resultImage(nullptr),
      imageProcessor(imageProcessor),
      screenSize(screenSize),
      settings(settings),
      encryption(encryption),
      processingId(0),
      appendUrl(appendUrl),
      appendReply(),
      appendRetries(0),
      pageData()
{
    QObject::connect(imageProcessor, &ImageProcessor::finished, this,
                     &DownloadScaledImageJob::processingFinished);
}

DownloadScaledImageJob::~DownloadScaledImageJob()
{
    // the QNetworkAccessManager deletes its replies when it is destroyed first
    if (!managerGuard)
        (void)appendReply.release();
}

void DownloadScaledImageJob::downloadFileReadyRead()
{
    // the data is read when the download finished, it is processed before it's saved
}

void DownloadScaledImageJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.close();
        file.remove();
    }

    if (followRedirect())
        return;

    if (reply->error() != QNetworkReply::NoError)
    {
        onError(QNetworkReply::NetworkError());
        return;
    }

    if (!appendUrl.isEmpty())
    {
        pageData = reply->readAll();
        downloadAppendix();
        return;
    }

    process({});
}

void DownloadScaledImageJob::restart()
{
    appendReply.reset();
    appendRetries = 0;
    pageData.clear();
    DownloadFileJob::restart();
}

void DownloadScaledImageJob::downloadAppendix()
{
    retryHold.reset();

    QNetworkRequest request = createRequest(appendUrl);
    request.setTransferTimeout(20000);

    appendReply.reset(networkManager->get(request));
    trackReply(appendReply.get());

    QObject::connect(appendReply.get(), &QNetworkReply::finished, this,
                     &DownloadScaledImageJob::appendixFinished);
    QObject::connect(appendReply.get(), &QNetworkReply::sslErrors, this, &DownloadJobBase::onSslErrors);
}

void DownloadScaledImageJob::appendixFinished()
{
    if (appendReply->error() == QNetworkReply::NoError)
    {
        process(appendReply->readAll());
        return;
    }

    int status = appendReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool transient = status == 0 || status == 408 || status == 429 || status >= 500;
    if (transient && appendRetries++ < 1)
    {
        retryHold = holdActivity();
        // not right away: this runs in a signal of the reply that downloadAppendix() deletes
        QTimer::singleShot(1000, this, &DownloadScaledImageJob::downloadAppendix);
        return;
    }

    // Without an answer (no connection, a timeout) the whole page fails: saved without it, a later download
    // wouldn't try again.
    if (status == 0)
    {
        errorString = "Couldn't download the rest of the page: " + appendReply->errorString();
        qDebug() << "Error string:" << errorString;
        emit downloadError();
        return;
    }

    // some of them are missing at the source, the page is still readable without it
    qDebug() << "Page appendix not loaded, using the page only:" << appendUrl << appendReply->errorString();
    process({});
}

void DownloadScaledImageJob::process(const QByteArray &appendData)
{
    // the image is processed on the image processor's thread, see processingFinished()
    ImageProcessingRequest request;
    request.data = pageData.isEmpty() ? reply->readAll() : pageData;
    request.appendData = appendData;
    pageData.clear();
    request.filepath = filepath;
    request.screenSize = screenSize;
    request.doublePageMode = settings->doublePageMode;
    request.trim = settings->trimPages;
    request.manhwaMode = settings->manhwaMode;
    request.encryption = encryption;

    processingId = imageProcessor->process(request);
}

void DownloadScaledImageJob::processingFinished(quint64 id, const QImage &image)
{
    if (id != processingId)
        return;

    if (!image.isNull())
    {
        resultImage.reset(new QImage(image));
        isCompleted = true;
        emit completed();
    }
    else
    {
        errorString = "Failed to load or process image.";
        emit downloadError();
    }
}
