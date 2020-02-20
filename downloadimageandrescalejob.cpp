#include "downloadimageandrescalejob.h"

#include <QImage>
#include <QtConcurrent/QtConcurrent>

#include "configs.h"
#include "downloadmanager.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QObject *parent, QNetworkAccessManager *networkManager, const QString &url,
    const QString &path, QSize size)
    : DownloadFileJob(parent, networkManager, url, path), size(size), array()
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
        file.flush();
        file.close();
        file.remove();
    }

    if (reply == nullptr)
    {
        errorString += " reply is nullptr";
        emit downloadError();
        return;
    }
    if (reply->error() != QNetworkReply::NoError)
    {
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        file.close();
        //        connect(&watcher, SIGNAL(finished()), this,
        //        SLOT(rescaleImageFinised()));

        array = reply->readAll();
        if (array.length() > 0)
        {
            //            QFuture<void> conc = QtConcurrent::run(this,
            //            &DownloadScaledImageJob::rescaleImage, array,
            //            filepath); watcher.setFuture(conc);
            rescaleImage(array, filepath);
            rescaleImageFinised();
        }
        if (reply != nullptr) reply->deleteLater();
        reply = nullptr;
    }
}

void DownloadScaledImageJob::rescaleImageFinised()
{
    array.clear();
    isCompleted = true;
    emit completed();
}

void DownloadScaledImageJob::rescaleImage(const QByteArray &array,
                                          const QString &filename)
{
    QImage img;
    img.loadFromData(array);
    img = img.scaled(size.width(), size.height(), Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_Grayscale8);
    //    img.save(filename, "PNG");
    img.save(filename);
    //    qDebug() << "rescaled!";
}
