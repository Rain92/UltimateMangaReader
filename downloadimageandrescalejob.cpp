#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, const QString &url, const QString &path, QSize imgSize,
    bool flipVerticalImage, bool trimImage,
    const QList<std::tuple<const char *, const char *>> &customHeaders)
    : DownloadFileJob(networkManager, url, path, customHeaders),
      imgSize(imgSize),
      trimImage(trimImage),
      flipVerticalImage(flipVerticalImage)
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
            errorString = "Failed to load or rescale image.";
            emit downloadError();
        }
    }
}

QRect DownloadScaledImageJob::getTrimRect(const QImage &img)
{
    const uchar threshold = 240;

    int bottom = 0;
    int top = 0;
    bool contentReached = false;
    int leftMin = img.width();
    int rightMin = img.width();
    for (int h = 0; h < img.height(); h++)
    {
        const uchar *line = img.constScanLine(h);

        bool allWhite = true;
        int left = 0;
        int right = 0;

        for (int w = 0; w < img.width(); w++)
        {
            if (line[w] > threshold)
            {
                if (allWhite)
                    left++;
                else
                    right++;
            }
            else if (!allWhite)
            {
                right = 0;
            }
            else
            {
                allWhite = false;
                contentReached = true;
                right = 0;
            }
        }
        if (allWhite)
        {
            if (!contentReached)
                top++;
            else
                bottom++;
        }
        else
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMin(rightMin, right);

            bottom = 0;
        }
    }
    return QRect(leftMin, top, img.width() - rightMin - leftMin, img.height() - bottom - top);
}

QImage DownloadScaledImageJob::rescaleImage(const QImage &img)
{
    auto rsize = imgSize;
    if (flipVerticalImage && (img.width() <= img.height()) != (imgSize.width() <= imgSize.height()))
        rsize.transpose();

    return img.scaled(rsize.width(), rsize.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

bool DownloadScaledImageJob::processImage(const QByteArray &array)
{
    //    QElapsedTimer t;
    //    t.start();

    QImage img;
    if (!img.loadFromData(array))
        return false;

    if (!trimImage)
        img = rescaleImage(img);

    auto greyImg = img.convertToFormat(QImage::Format_Grayscale8);
    bool res = false;

    if (!greyImg.isNull())
    {
        if (trimImage)
        {
            auto trimRect = getTrimRect(greyImg);
            greyImg = greyImg.copy(trimRect);

            greyImg = rescaleImage(greyImg);
        }
        res = greyImg.save(filepath);
    }

    if (!res)  // something went wrong with the greyscale img; use original
    {
        img = rescaleImage(img);
        res = img.save(filepath);
    }
    //    qDebug() << t.elapsed();

    return res || QFileInfo(filepath).size() > 0;
}
