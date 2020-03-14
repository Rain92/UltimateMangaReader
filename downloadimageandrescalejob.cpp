#include "downloadimageandrescalejob.h"

#include <QImage>

#include "defines.h"
#include "downloadmanager.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QObject *parent, QNetworkAccessManager *networkManager, const QString &url,
    const QString &path, QSize size)
    : DownloadFileJob(parent, networkManager, url, path), size(size)
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

    QUrl redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
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
            //            QFile::remove(filepath);
            emit downloadError();
        }
    }
}

bool DownloadScaledImageJob::rescaleImage(const QByteArray &array)
{
    QImage img;
    if (!img.loadFromData(array))
        return false;
    img = img.convertToFormat(QImage::Format_Grayscale8);
    img = img.scaled(size.width(), size.height(), Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);
    if (!img.save(filepath))
        return QFileInfo(filepath).size() > 0;

    return true;
}
