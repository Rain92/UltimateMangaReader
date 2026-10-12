#ifndef TRIMRECT_H
#define TRIMRECT_H

#include <QByteArray>
#include <QRect>

// Bounding box of the non white content (pixels <= 234) of an 8 bit grayscale image,
// its width rounded up to a multiple of 4.
QRect getTrimRect(const QByteArray &buffer, int imgWidth, int imgHeight, int stride);

#endif  // TRIMRECT_H
