#include "downloadimageandrescalejob.h"

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, const QString &url, const QString &path, QSize screenSize,
    Settings *settings, const QList<std::tuple<const char *, const char *>> &customHeaders,
    const EncryptionDescriptor encryption)
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

inline QPair<int, int> getTrimRectHelper(const QImage &img, int lineNum, int limitLeft, int limitRight,
                                         const uchar threshold)
{
    const uchar *line = img.constScanLine(lineNum);

    int left = 0;
    int right = 0;

    for (left = 0; left < limitLeft; left++)
        if (line[left] <= threshold)
            break;

    for (right = img.width() - 1; right >= limitRight; right--)
        if (line[right] <= threshold)
            break;

    return {left, right};
}

QRect getTrimRect(const QImage &img)
{
    const uchar threshold = 234;

    int bottom = 0;
    int top = 0;
    int leftMin = img.width();
    int rightMin = 0;

    for (top = 0; top < img.height(); top++)
    {
        auto [left, right] = getTrimRectHelper(img, top, img.width(), rightMin, threshold);

        bool allWhite = left == img.width();
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
            break;
        }
    }

    for (bottom = img.height() - 1; bottom >= top; bottom--)
    {
        auto [left, right] = getTrimRectHelper(img, bottom, img.width(), rightMin, threshold);

        bool allWhite = left == img.width();
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
            break;
        }
    }

    for (int middle = top; middle < bottom; middle++)
    {
        auto [left, right] = getTrimRectHelper(img, middle, leftMin, rightMin, threshold);

        bool allWhite = left == img.width();
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
        }
    }

    return QRect(leftMin, top, rightMin - leftMin, bottom - top);
}

QImage DownloadScaledImageJob::rescaleImage(const QImage &img)
{
    auto rescaleSize = screenSize;

    if (settings->doublePageFullscreen &&
        (img.width() <= img.height()) != (screenSize.width() <= screenSize.height()))
    {
        auto imgR = img.transformed(QTransform().rotate(90));
        return imgR.scaled(rescaleSize.width(), rescaleSize.height(), Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
    }

    if (settings->manhwaMode &&
        ((float)img.height() / img.width()) > 1.6 * ((float)screenSize.height() / screenSize.width()))
        rescaleSize = QSize(screenSize.width(), ((float)screenSize.width()) / img.width() * img.height());

    return img.scaled(rescaleSize.width(), rescaleSize.height(), Qt::KeepAspectRatio,
                      Qt::SmoothTransformation);
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

    QImage img;
    if (!img.loadFromData(array))
        return false;

    auto greyImg = img.convertToFormat(QImage::Format_Grayscale8);
    bool res = false;

    if (!greyImg.isNull())
    {
        if (settings->trimPages)
        {
            auto trimRect = getTrimRect(greyImg);
            greyImg = greyImg.copy(trimRect);
        }
        resultImage.reset(new QImage(rescaleImage(greyImg)));
        res = resultImage->save(filepath, nullptr, 80);
    }

    // if something went wrong with the greyscale img -> use original
    if (!res)
    {
        resultImage.reset(new QImage(rescaleImage(img)));
        res = resultImage->save(filepath, nullptr, 80);
    }

    //    qDebug() << "Image processing:" << t.elapsed();

    return res || QFileInfo(filepath).size() > 0;
}
