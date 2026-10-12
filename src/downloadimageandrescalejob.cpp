#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, ImageProcessor *imageProcessor, const QString &url,
    const QString &path, QSize screenSize, Settings *settings,
    const QList<std::tuple<const char *, const char *>> &customHeaders, const EncryptionDescriptor &encryption)
    : DownloadFileJob(networkManager, url, path, customHeaders),
      resultImage(nullptr),
      imageProcessor(imageProcessor),
      screenSize(screenSize),
      settings(settings),
      encryption(encryption),
      processingId(0)
{
    QObject::connect(imageProcessor, &ImageProcessor::finished, this,
                     &DownloadScaledImageJob::processingFinished);
}

void DownloadScaledImageJob::downloadFileReadyRead()
{
    // don't save to file because its gonna be rescaled anyway
    //    file.write(reply->readAll());
}

void DownloadScaledImageJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.close();
        file.remove();
    }

    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid() && reply->url() != redirect)
    {
        this->url = redirect.toString();
        this->restart();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        onError(QNetworkReply::NetworkError());
        return;
    }

    // the image is processed on the image processor's thread, see processingFinished()
    ImageProcessingRequest request;
    request.data = reply->readAll();
    request.filepath = filepath;
    request.screenSize = screenSize;
    request.doublePageMode = settings->doublePageMode;
    request.trim = settings->trimPages;
    request.manhwaMode = settings->manhwaMode;
    request.swDither = settings->ditheringMode >= SWDithering;
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
