#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(QNetworkAccessManager *networkManager,
                                               ImageProcessor *imageProcessor, const QString &url,
                                               const QString &path, QSize screenSize, Settings *settings,
                                               const HttpHeaders &headers, const EncryptionDescriptor &encryption,
                                               const QString &appendUrl)
    : DownloadFileJob(networkManager, url, path, headers),
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

QString DownloadScaledImageJob::prepareTarget()
{
    // the image processor saves the page
    QDir().mkpath(QFileInfo(filepath).path());
    return {};
}

void DownloadScaledImageJob::readyRead()
{
    // the data is read when the download finished, it is processed before it's saved
}

void DownloadScaledImageJob::replySucceeded()
{
    pageData = reply->readAll();

    if (!appendUrl.isEmpty())
    {
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
    processingId = 0;
    DownloadFileJob::restart();
}

void DownloadScaledImageJob::downloadAppendix()
{
    retryHold.reset();

    QNetworkRequest request = createRequest(appendUrl, headers);
    request.setTransferTimeout(20000);

    appendReply.reset(networkManager->get(request));
    prepareReply(appendReply.get());

    QObject::connect(appendReply.get(), &QNetworkReply::finished, this,
                     &DownloadScaledImageJob::appendixFinished);
}

void DownloadScaledImageJob::appendixFinished()
{
    if (appendReply->error() == QNetworkReply::NoError)
    {
        process(appendReply->readAll());
        return;
    }

    if (isTransientError(appendReply.get()) && appendRetries++ < 1)
    {
        retryHold = holdActivity();
        // not right away: this runs in a signal of the reply that downloadAppendix() deletes
        QTimer::singleShot(1000, this, &DownloadScaledImageJob::downloadAppendix);
        return;
    }

    // Without an answer (no connection, a timeout) the whole page fails: saved without it, a later download
    // wouldn't try again.
    int status = appendReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0)
    {
        finishFailed("Couldn't download the rest of the page: " + appendReply->errorString());
        return;
    }

    // some of them are missing at the source, the page is still readable without it
    qDebug() << "Page appendix not loaded, using the page only:" << appendUrl << appendReply->errorString();
    process({});
}

void DownloadScaledImageJob::process(const QByteArray &appendData)
{
    retryHold.reset();

    // the image is processed on the image processor's thread, see processingFinished()
    ImageProcessingRequest request;
    request.data = pageData;
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
    if (id != processingId || isFinished())
        return;

    if (!image.isNull())
    {
        resultImage.reset(new QImage(image));
        finishCompleted();
    }
    else
    {
        finishFailed("Failed to load or process image.");
    }
}
