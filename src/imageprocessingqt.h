#ifndef IMAGEPROCESSINGQT_H
#define IMAGEPROCESSINGQT_H

#include <turbojpeg.h>

#include <QDebug>
#include <QImage>

#include "enums.h"

inline QPair<int, int> getTrimRectHelper(const uchar *linePtr, int imgWidth, int limitLeft, int limitRight,
                                         const uchar threshold);

QRect getTrimRect(const QByteArray &buffer, int imgWidth, int imgHeight, int stride);

QSize calcRescaleSize(QSize imgSize, QSize screenSize, bool doublePageFullscreen, bool manhwaMode);

int calcRotationInfo(QSize imgSize, QSize screenSize, DoublePageMode doublePageMode);

QImage rescaleImageQt(const QImage &img, QSize rescaleSize, bool rot90);

QImage processImageQt(const QByteArray &array, const QString &filepath, QSize screenSize,
                      DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool useSWDither);

#endif  // IMAGEPROCESSINGQT_H
