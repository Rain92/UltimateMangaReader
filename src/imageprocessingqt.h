#pragma once

#include <QImage>

#include "enums.h"

QSize calcRescaleSize(QSize imgSize, QSize screenSize, bool doublePageFullscreen, bool manhwaMode);

int calcRotationInfo(QSize imgSize, QSize screenSize, DoublePageMode doublePageMode);

QImage rescaleImageQt(const QImage &img, QSize rescaleSize, bool rot90);

QImage processImageQt(const QByteArray &array, QSize screenSize, DoublePageMode doublePageMode, bool trim,
                      bool manhwaMode);
