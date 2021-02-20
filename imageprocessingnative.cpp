
#include "imageprocessingnative.h"

#define JPEG_MAGIC_NUMBER_0 (char)0xFF
#define JPEG_MAGIC_NUMBER_1 (char)0xD8

#define PNG_MAGIC_NUMBER_0 (char)0x89
#define PNG_MAGIC_NUMBER_1 (char)0x50

bool isJpeg(const QByteArray &buffer)
{
    return buffer.size() > 2 && buffer.data()[0] == JPEG_MAGIC_NUMBER_0 &&
           buffer.data()[1] == JPEG_MAGIC_NUMBER_1;
}

bool isPng(const QByteArray &buffer)
{
    return buffer.size() > 2 && buffer.data()[0] == PNG_MAGIC_NUMBER_0 &&
           buffer.data()[1] == PNG_MAGIC_NUMBER_1;
}

QImage loadQImageFast(const QString &path, bool useSWDithering)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QImage();

    auto buffer = file.readAll();
    file.close();

    GreyscaleImage img;

    if (isPng(buffer))
    {
        img.loadFromPng(buffer);
    }
    else if (isJpeg(buffer))
    {
        img.loadFromJpeg(buffer);
    }

    if (img.isNull())
    {
        QImage ret(path);

        if (useSWDithering && ret.isNull() && ret.format() == QImage::Format_Grayscale8 &&
            ret.bytesPerLine() == ret.width())
        {
            auto imgbuffer = QByteArray::fromRawData((const char *)ret.bits(), ret.sizeInBytes());
            ditherBuffer(imgbuffer, ret.width(), ret.height());
        }

        return ret;
    }

    if (useSWDithering)
        img.dither();

    return img.toQImage();
}

GreyscaleImage loadFromJpegAndRotate(const QByteArray &buffer, QSize screenSize, bool doublePageFullscreen,
                                     bool &rot90)
{
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
    int inSubsamp, inColorspace;
    int width, height;

    tjhandle tjInstanceD = tjInitDecompress(), tjInstanceT = tjInitTransform();

    GreyscaleImage img;

    auto handleguard = qScopeGuard([&] {
        if (tjInstanceD)
            tjDestroy(tjInstanceD);
        if (tjInstanceT)
            tjDestroy(tjInstanceT);
    });

    if (tjDecompressHeader3(tjInstanceD, (uchar *)buffer.data(), buffer.size(), &width, &height, &inSubsamp,
                            &inColorspace) < 0)
        return img;

    rot90 = calcRotationInfo(QSize(width, height), screenSize, doublePageFullscreen);

    if (rot90)
    {
        unsigned char *dstBuf = nullptr; /* Dynamically allocate the JPEG buffer */
        unsigned long dstSize = 0;

        tjtransform xform;
        memset(&xform, 0, sizeof(tjtransform));
        xform.options = TJXOPT_GRAY | TJXOPT_TRIM;
        xform.op = TJXOP_ROT90;

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
                     bool doublePageFullscreen, bool trim, bool manhwaMode, bool useSWDither)
{
    GreyscaleImage img;

    bool rot90 = false;

    if (isPng(buffer))
    {
        img.loadFromPng(buffer);

        rot90 = calcRotationInfo(img.size(), screenSize, doublePageFullscreen);

        if (rot90)
            img = img.rotate(90);
    }
    else if (isJpeg(buffer))
    {
        img = loadFromJpegAndRotate(buffer, screenSize, doublePageFullscreen, rot90);

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

        img = img.crop(trimRect);
    }

    auto rescaleSize = calcRescaleSize(img.size(), screenSize, rot90, manhwaMode);

    img = img.resize(rescaleSize);

    if (!img.saveAsJpeg(filepath))
        return QImage();

    if (useSWDither)
        img.dither();

    QImage retimg = img.toQImage();

    return retimg;
}
