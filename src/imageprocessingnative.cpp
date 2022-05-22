
#include "imageprocessingnative.h"

QImage loadQImageFast(const QString &path, bool useSWDithering)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "loadQImageFast failed for image:" << path;
        return QImage();
    }

    auto buffer = file.readAll();
    file.close();

    GreyscaleImage img;

    img.loadFromEncoded(buffer);

    if (img.isNull())
    {
        QImage ret(path);

        return ret;
    }

    return img.toQImage();
}

GreyscaleImage loadFromJpegAndRotate(const QByteArray &buffer, QSize screenSize,
                                     DoublePageMode doublePageMode, int &rot90)
{
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
    int inSubsamp, inColorspace;
    int width, height;

    tjhandle tjInstanceD = tjInitDecompress(), tjInstanceT = tjInitTransform();

    GreyscaleImage img;

    auto handleguard = qScopeGuard(
        [&]
        {
            if (tjInstanceD)
                tjDestroy(tjInstanceD);
            if (tjInstanceT)
                tjDestroy(tjInstanceT);
        });

    if (tjDecompressHeader3(tjInstanceD, (uchar *)buffer.data(), buffer.size(), &width, &height, &inSubsamp,
                            &inColorspace) < 0)
        return img;

    rot90 = calcRotationInfo(QSize(width, height), screenSize, doublePageMode);

    if (rot90 != 0)
    {
        unsigned char *dstBuf = nullptr; /* Dynamically allocate the JPEG buffer */
        unsigned long dstSize = 0;

        tjtransform xform;
        memset(&xform, 0, sizeof(tjtransform));
        xform.options = TJXOPT_GRAY | TJXOPT_TRIM;
        if (rot90 == 90)
            xform.op = TJXOP_ROT90;
        if (rot90 == -90)
            xform.op = TJXOP_ROT270;

        if (tjTransform(tjInstanceT, (uchar *)buffer.data(), buffer.size(), 1, &dstBuf, &dstSize, &xform,
                        flags) < 0)
        {
            tjFree(dstBuf);
            return img;
        }
        auto nbuffer = QByteArray::fromRawData((char *)dstBuf, dstSize);

        img.loadFromJpeg(nbuffer);
        tjFree(dstBuf);
    }
    else
    {
        img.loadFromJpeg(buffer);
    }

    return img;
}

QImage processImageN(const QByteArray &buffer, const QString &filepath, QSize screenSize,
                     DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool useSWDither)
{
    GreyscaleImage img;

    int rot90 = 0;

    if (isPng(buffer))
    {
        img.loadFromPng(buffer);

        if (!img.isValid())
            return QImage();

        rot90 = calcRotationInfo(img.size(), screenSize, doublePageMode);

        if (rot90 != 0)
            img = img.rotate(rot90);
    }
    else if (isJpeg(buffer))
    {
        img = loadFromJpegAndRotate(buffer, screenSize, doublePageMode, rot90);

        if (!img.isValid())
            return QImage();
    }
    else
    {
        return QImage();
    }

    if (trim)
    {
        auto trimRect = getTrimRect(img.buffer, img.width, img.height, img.width);

        if (trimRect.isValid())
            img = img.crop(trimRect);
    }

    auto rescaleSize = calcRescaleSize(img.size(), screenSize, rot90 != 0, manhwaMode);

    img = img.resize(rescaleSize);

    if (filepath != "")
        if (!img.saveAsJpeg(filepath))
            return QImage();

    QImage retimg = img.toQImage();

    return retimg;
}
