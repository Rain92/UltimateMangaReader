#include "downloadimageandrescalejob.h"
#include "downloadmanager.h"
#include <QImage>
#include <QPixmap>
#include <QtConcurrent/QtConcurrent>
#include "configs.h"

//#include <QtTest/QTest>



DownloadScaledImageJob::DownloadScaledImageJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, const QString &path, int width, int height) :
    DownloadFileJob(parent, nm, url, path),
    width(width),
    height(height),
    array()
{
}


void DownloadScaledImageJob::downloadFileReadyRead()
{
    //don't save to file bcs its gonna be rescaled anyway
//    file.write(reply->readAll());
}

void DownloadScaledImageJob::downloadFileFinished()
{
//    qDebug() << "download finished rescaler:" << url;

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
    if (QNetworkReply::NoError != reply->error() )
    {
        file.close();
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        file.close();
        connect(&watcher, SIGNAL(finished()), this, SLOT(rescaleImageFinised()));

        array = reply->readAll();
        if (array.length() > 0)
        {
            QFuture<void> conc = QtConcurrent::run(this, &DownloadScaledImageJob::rescaleImage, array, filepath);
            watcher.setFuture(conc);
        }
        if (reply != nullptr)
            reply->deleteLater();
        reply = nullptr;
    }
}

void DownloadScaledImageJob::rescaleImageFinised()
{
    array.clear();
    isCompleted = true;
    emit completed();

}


void DownloadScaledImageJob::rescaleImage(QByteArray array, const QString &filename)
{
    QImage img;
    img.loadFromData(array);
    img = img.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_Grayscale8);
    img.save(filename, "PNG");
}
