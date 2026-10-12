#include "imageprocessingnative.h"

#include "perftimer.h"

QImage loadQImageFast(const QString &path, bool)
{
    PerfTimer timer("loadQImageFast");

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "loadQImageFast failed for image:" << path;
        return QImage();
    }

    auto buffer = file.readAll();
    file.close();
    timer.lap("read");

    QImage img = decodeToQImage(buffer);

    if (img.isNull())
        return QImage(path);

    return img;
}

// The size a JPEG has to be decoded at so that the page is never upscaled afterwards.
static QSize minDecodeSize(QSize jpegSize, QSize screenSize, int rot90, bool trim, bool manhwaMode)
{
    QSize rotated = rot90 != 0 ? jpegSize.transposed() : jpegSize;
    QSize target = calcRescaleSize(rotated, screenSize, rot90 != 0, manhwaMode);

    // trimmed pages end up being rescaled by a larger factor, so leave some headroom
    if (trim)
        target = QSize(qRound(target.width() * 1.1), qRound(target.height() * 1.1));

    return rot90 != 0 ? target.transposed() : target;
}

static QImage finishPage(GreyscaleImage &img, int rot90, const QString &filepath, QSize screenSize,
                         bool trim, bool manhwaMode, PerfTimer &timer);

QImage processImageN(const QByteArray &buffer, const QString &filepath, QSize screenSize,
                     DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool useSWDither)
{
    Q_UNUSED(useSWDither)
    PerfTimer timer("processImageN");

    GreyscaleImage img;

    int rot90 = 0;

    if (isPng(buffer))
    {
        if (!img.loadFromPng(buffer) || !img.isValid())
            return QImage();

        rot90 = calcRotationInfo(img.size(), screenSize, doublePageMode);
    }
    else if (isJpeg(buffer))
    {
        QSize jpegSize;
        if (!readJpegSize(buffer, jpegSize))
            return QImage();

        rot90 = calcRotationInfo(jpegSize, screenSize, doublePageMode);

        // Decode reduced in size right away, the decoder's DCT scaling is cheaper than
        // decoding everything and resizing afterwards.
        QSize minSize = minDecodeSize(jpegSize, screenSize, rot90, trim, manhwaMode);
        if (!img.loadFromJpeg(buffer, minSize) || !img.isValid())
            return QImage();
    }
    else if (isWebp(buffer))
    {
        QSize webpSize;
        if (!readWebpSize(buffer, webpSize))
            return QImage();

        rot90 = calcRotationInfo(webpSize, screenSize, doublePageMode);

        if (!img.loadFromWebp(buffer) || !img.isValid())
            return QImage();
    }
    else
    {
        return QImage();
    }
    timer.lap("decode");

    return finishPage(img, rot90, filepath, screenSize, trim, manhwaMode, timer);
}

QImage processDecodedImage(const QImage &decoded, const QString &filepath, QSize screenSize,
                           DoublePageMode doublePageMode, bool trim, bool manhwaMode)
{
    PerfTimer timer("processDecodedImage");

    if (decoded.isNull())
        return QImage();

    QImage grey = decoded.format() == QImage::Format_Grayscale8
                      ? decoded
                      : decoded.convertToFormat(QImage::Format_Grayscale8);
    if (grey.isNull())
        return QImage();
    timer.lap("togrey");

    // GreyscaleImage buffers have no line padding
    const int width = grey.width();
    QByteArray packed(width * grey.height(), Qt::Uninitialized);
    for (int y = 0; y < grey.height(); y++)
        memcpy(packed.data() + y * width, grey.constScanLine(y), width);

    GreyscaleImage img(grey.size(), std::move(packed));
    int rot90 = calcRotationInfo(img.size(), screenSize, doublePageMode);
    return finishPage(img, rot90, filepath, screenSize, trim, manhwaMode, timer);
}

// Rotates, trims, rescales and saves a decoded page: the part of the pipeline all decoders share.
static QImage finishPage(GreyscaleImage &img, int rot90, const QString &filepath, QSize screenSize,
                         bool trim, bool manhwaMode, PerfTimer &timer)
{
    if (rot90 != 0)
    {
        img = img.rotate(rot90);
        timer.lap("rotate");
    }

    if (trim)
    {
        auto trimRect = getTrimRect(img.buffer, img.width, img.height, img.width);

        if (trimRect.isValid())
            img = img.crop(trimRect);
        timer.lap("trim");
    }

    auto rescaleSize = calcRescaleSize(img.size(), screenSize, rot90 != 0, manhwaMode);

    QImage retimg = img.resizeToQImage(rescaleSize);
    timer.lap("resize");
    if (retimg.isNull())
        return retimg;

    if (filepath != "")
    {
        if (!saveQImageAsJpeg(retimg, filepath))
            return QImage();
        timer.lap("save");
    }

    return retimg;
}
