#ifndef IMAGEPROCESSINGQT_H
#define IMAGEPROCESSINGQT_H

#include <turbojpeg.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <cstring>

inline QPair<int, int> getTrimRectHelper(const uchar *linePtr, int imgWidth, int limitLeft, int limitRight,
                                         const uchar threshold);

QRect getTrimRect(const QByteArray &buffer, int imgWidth, int imgHeight, int stride);

QSize calcRescaleSize(QSize imgSize, QSize screenSize, bool doublePageFullscreen, bool manhwaMode);

bool calcRotationInfo(QSize imgSize, QSize screenSize, bool doublePageFullscreen);

QImage rescaleImageQt(const QImage &img, QSize rescaleSize, bool rot90);

QImage processImageQt(const QByteArray &array, const QString &filepath, QSize screenSize,
                      bool doublePageFullscreen, bool trim, bool manhwaMode);

#endif  // IMAGEPROCESSINGQT_H
