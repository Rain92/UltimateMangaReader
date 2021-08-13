
#include "imageprocessingqt.h"

inline QPair<int, int> getTrimRectHelper(const uchar *linePtr, int imgWidth, int limitLeft, int limitRight,
                                         const uchar threshold)
{
    int left = 0;
    int right = 0;

    for (left = 0; left < limitLeft; left++)
        if (linePtr[left] <= threshold)
            break;

    for (right = imgWidth - 1; right >= limitRight; right--)
        if (linePtr[right] <= threshold)
            break;

    return {left, right};
}

QRect getTrimRect(const QByteArray &buffer, int imgWidth, int imgHeight, int stride)
{
    const uchar threshold = 234;

    int bottom = 0;
    int top = 0;
    int leftMin = imgWidth;
    int rightMin = 0;

    uchar *linePtr;

    for (top = 0; top < imgHeight; top++)
    {
        linePtr = (uchar *)buffer.data() + top * stride;
        auto [left, right] = getTrimRectHelper(linePtr, imgWidth, imgWidth, rightMin, threshold);

        bool allWhite = left == imgWidth;
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
            break;
        }
    }

    for (bottom = imgHeight - 1; bottom >= top; bottom--)
    {
        linePtr = (uchar *)buffer.data() + bottom * stride;
        auto [left, right] = getTrimRectHelper(linePtr, imgWidth, imgWidth, rightMin, threshold);

        bool allWhite = left == imgWidth;
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
            break;
        }
    }

    for (int middle = top; middle < bottom; middle++)
    {
        linePtr = (uchar *)buffer.data() + middle * stride;
        auto [left, right] = getTrimRectHelper(linePtr, imgWidth, imgWidth, rightMin, threshold);

        bool allWhite = left == imgWidth;
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
        }
    }

    int width = rightMin - leftMin + 1;
    int height = bottom - top + 1;
    int ntw = qMax(0, width + 4 - width % 4);
    if (ntw + rightMin > imgWidth)
        ntw -= 4;

    return QRect(leftMin, top, ntw, height);
}

int calcRotationInfo(QSize imgSize, QSize screenSize, DoublePageMode doublePageMode)
{
    bool rot90 = (imgSize.width() <= imgSize.height()) != (screenSize.width() <= screenSize.height());

    if (rot90 && doublePageMode == DoublePage90CW)
        return 90;
    else if (rot90 && doublePageMode == DoublePage90CCW)
        return -90;

    return 0;
}

QSize fitToSize(QSize imgSize, QSize maxSize)
{
    bool fitToWidth = (float)maxSize.width() / imgSize.width() < (float)maxSize.height() / imgSize.height();
    if (fitToWidth)
    {
        return QSize(maxSize.width(), ((float)imgSize.height() * maxSize.width()) / imgSize.width());
    }
    else  // fit to height
    {
        return QSize((float)(imgSize.width() * maxSize.height()) / imgSize.height(), maxSize.height());
    }
}

QSize calcRescaleSize(QSize imgSize, QSize screenSize, bool rot90, bool manhwaMode)
{
    QSize rescaleSize;

    if (rot90)
    {
        rescaleSize = fitToSize(imgSize, screenSize);
    }
    else
    {
        if (manhwaMode && ((float)imgSize.height() / imgSize.width()) >
                              1.6 * ((float)screenSize.height() / screenSize.width()))
            rescaleSize =
                QSize(screenSize.width(), (imgSize.height() * screenSize.width()) / imgSize.width());
        else
        {
            rescaleSize = fitToSize(imgSize, screenSize);
        }
    }
    rescaleSize.setWidth(rescaleSize.width() + 4 - rescaleSize.width() % 4);
    return rescaleSize;
}

QImage processImageQt(const QByteArray &array, const QString &filepath, QSize screenSize,
                      DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool useSWDither)
{
    QImage img;
    if (!img.loadFromData(array))
        return QImage();

    auto rot90 = calcRotationInfo(img.size(), screenSize, doublePageMode);

    QImage ret;
    QImage greyImg = img.convertToFormat(QImage::Format_Grayscale8);
    bool res = false;

    if (!greyImg.isNull())
    {
        if (trim)
        {
            auto arrayT = QByteArray::fromRawData((const char *)greyImg.bits(), greyImg.sizeInBytes());
            auto trimRect = getTrimRect(arrayT, img.width(), img.height(), greyImg.bytesPerLine());
            greyImg = greyImg.copy(trimRect);
        }

        if (rot90 != 0)
            greyImg = greyImg.transformed(QTransform().rotate(rot90));
        auto rescaleSize = calcRescaleSize(greyImg.size(), screenSize, rot90 != 0, manhwaMode);

        ret = greyImg.scaled(rescaleSize.width(), rescaleSize.height(), Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
        res = ret.save(filepath, nullptr, 85);
    }

    // if something went wrong with the greyscale img -> use original
    if (!res)
    {
        if (rot90 != 0)
            img = img.transformed(QTransform().rotate(rot90));
        auto rescaleSize = calcRescaleSize(img.size(), screenSize, rot90 != 0, manhwaMode);

        ret = img.scaled(rescaleSize.width(), rescaleSize.height(), Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
        res = ret.save(filepath, nullptr, 85);
    }

    if (res)
    {
        if (useSWDither && ret.isNull() && ret.format() == QImage::Format_Grayscale8 &&
            ret.bytesPerLine() == ret.width())
        {
            auto buffer = QByteArray::fromRawData((const char *)ret.bits(), ret.sizeInBytes());
            ditherBuffer(buffer, ret.width(), ret.height());
        }

        return ret;
    }

    return QImage();
}
