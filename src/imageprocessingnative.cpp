#include "imageprocessingnative.h"

#include <QFile>
#include <cmath>

#include "greyscaleimage.h"
#include "imageprocessingqt.h"
#include "perftimer.h"
#include "trimrect.h"

QImage loadQImageFast(const QString &path)
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
                     DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool *decoded)
{
    PerfTimer timer("processImageN");

    if (decoded)
        *decoded = false;

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

    if (decoded)
        *decoded = true;

    return finishPage(img, rot90, filepath, screenSize, trim, manhwaMode, timer);
}

QImage processStitchedImageN(const QByteArray &top, const QByteArray &bottom, const QString &filepath,
                             QSize screenSize, DoublePageMode doublePageMode, bool trim, bool manhwaMode)
{
    PerfTimer timer("processStitchedImageN");

    // decoded at full size, the parts only fit together that way
    GreyscaleImage upper, lower;
    if (!upper.loadFromEncoded(top) || !upper.isValid() || !lower.loadFromEncoded(bottom) || !lower.isValid())
        return QImage();

    if (upper.width != lower.width)
    {
        qDebug() << "Can't stitch images of different widths:" << upper.size() << lower.size();
        return QImage();
    }
    timer.lap("decode");

    // the buffers have no line padding, one below the other is just one after the other
    QSize size(upper.width, upper.height + lower.height);
    upper.buffer.append(lower.buffer);
    GreyscaleImage img(size, std::move(upper.buffer));
    timer.lap("stitch");

    int rot90 = calcRotationInfo(img.size(), screenSize, doublePageMode);
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

    // Long strips (manhwa) are told apart by their shape before trimming: the trimmed content of a page can
    // be a sliver (a gutter line on an empty page), as a strip it would become huge.
    QSize untrimmed = img.size();
    bool strip = manhwaMode && rot90 == 0 &&
                 calcRescaleSize(untrimmed, screenSize, false, true).height() > screenSize.height();

    if (trim)
    {
        auto trimRect = getTrimRect(img.buffer, img.width, img.height, img.width);

        // only margins: content that is a small part of the page isn't zoomed in on
        if (trimRect.isValid() && trimRect.width() * 3 >= img.width && trimRect.height() * 3 >= img.height)
            img = img.crop(trimRect);
        timer.lap("trim");
    }

    auto rescaleSize = calcRescaleSize(img.size(), screenSize, rot90 != 0, strip);

    // JPEG can't be taller than 65535 px, and very long strips would need too much memory
    const int maxHeight = 65500;
    const double maxPixels = 24e6;
    double shrink = qMin(1.0, double(maxHeight) / rescaleSize.height());
    shrink = qMin(shrink, std::sqrt(maxPixels / (double(rescaleSize.width()) * rescaleSize.height())));
    if (shrink < 1.0)
    {
        int width = qMax(16, int(rescaleSize.width() * shrink) & ~3);
        rescaleSize = QSize(width, qMax(1, int(rescaleSize.height() * shrink)));
    }

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
