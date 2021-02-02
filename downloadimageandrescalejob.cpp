#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, const QString &url, const QString &path, QSize screenSize,
    Settings *settings, const QList<std::tuple<const char *, const char *>> &customHeaders,
    const EncryptionDescriptor &encryption)
    : DownloadFileJob(networkManager, url, path, customHeaders),
      resultImage(nullptr),
      screenSize(screenSize),
      settings(settings),
      encryption(encryption)
{
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
    }
    else
    {
        if (processImage(reply->readAll()))
        {
            isCompleted = true;
            emit completed();
        }
        else
        {
            errorString = "Failed to load or process image.";
            emit downloadError();
        }
    }
}

bool DownloadScaledImageJob::processImage(QByteArray &&array)
{
    //    QElapsedTimer t;
    //    t.start();

    if (encryption.type == XorEncryption)
    {
#ifdef KOBO
        decryptXorInplace_NEON(array, encryption.key);
#else
        decryptXorInplace(array, encryption.key);
#endif
    }

    //    qDebug() << "Image processing decrypt:" << t.elapsed();
    QImage pimg;

    if (isJpeg(array) || isPng(array))
        pimg = processImageN(array, filepath, screenSize, settings->doublePageFullscreen, settings->trimPages,
                             settings->manhwaMode);
    if (!pimg.isNull())
    {
        resultImage.reset(new QImage(pimg));
    }
    else
    {
        qDebug() << "Fast decoding failed, using fallback!";

        pimg = processImageQt(array, filepath, screenSize, settings->doublePageFullscreen,
                              settings->trimPages, settings->manhwaMode);

        if (!pimg.isNull())
            resultImage.reset(new QImage(pimg));
    }

    //    qDebug() << "Image processing:" << t.elapsed();

    return !pimg.isNull();
}
