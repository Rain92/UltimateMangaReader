#include "imageprocessingqt.h"

#include <QDebug>

#include "trimrect.h"

#include "imageprocessingnative.h"

int calcRotationInfo(QSize imgSize, QSize screenSize, DoublePageMode doublePageMode)
{
    // strips (e.g. the rest of a page that a source cut in two) aren't double pages, rotated they'd become a
    // thin line across the screen
    const double maxDoublePageAspect = 2.5;
    int longSide = qMax(imgSize.width(), imgSize.height());
    if (longSide > maxDoublePageAspect * qMin(imgSize.width(), imgSize.height()))
        return 0;

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
    // a multiple of 4, but not wider than it fits: the reader centers it and would cut off its edges
    rescaleSize.setWidth(qMax(16, rescaleSize.width() & ~3));
    rescaleSize.setHeight(qMax(1, rescaleSize.height()));
    return rescaleSize;
}

QImage processImageQt(const QByteArray &array, QSize screenSize, DoublePageMode doublePageMode, bool trim,
                      bool manhwaMode)
{

    QImage img;
    if (!img.loadFromData(array))
        return QImage();

    // Qt's decoders only decode here; rotating, trimming and rescaling go through the native pipeline,
    // which is much faster than QImage::scaled() (that one works in 32 bit) and keeps the result in
    // Format_Grayscale8 so it can be blitted and dithered on the fast path.
    return processDecodedImage(img, screenSize, doublePageMode, trim, manhwaMode);
}
