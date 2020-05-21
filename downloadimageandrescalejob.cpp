#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, const QString &url, const QString &path, QSize imgSize,
    const QList<std::tuple<const char *, const char *>> &customHeaders)
    : DownloadFileJob(networkManager, url, path, customHeaders), imgSize(imgSize)
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
        if (rescaleImage(reply->readAll()))
        {
            isCompleted = true;
            emit completed();
        }
        else
        {
            errorString = "Failed to load or rescale image.";
            emit downloadError();
        }
    }
}

bool DownloadScaledImageJob::rescaleImage(const QByteArray &array)
{
    QElapsedTimer t;
    t.start();

    QImage img;
    if (!img.loadFromData(array))
        return false;

    auto rsize = imgSize;
    if ((img.width() <= img.height()) != (imgSize.width() <= imgSize.height()))
        rsize.transpose();

    img = img.scaled(rsize.width(), rsize.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    auto greyimg = img.convertToFormat(QImage::Format_Grayscale8);

    bool res = greyimg.save(filepath);

    if (!res)
        res = img.save(filepath);

    if (!res)
        return QFileInfo(filepath).size() > 0;

    return true;
}
