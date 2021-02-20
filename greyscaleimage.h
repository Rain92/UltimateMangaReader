#ifndef GREYSCALEIMAGE_H
#define GREYSCALEIMAGE_H

#include <png.h>
#include <turbojpeg.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <cstring>

#include "dither.h"
#include "imagerotate.h"
#include "thirdparty/simdimageresize.h"

class GreyscaleImage
{
public:
    GreyscaleImage();
    GreyscaleImage(QSize size);
    GreyscaleImage(QSize size, QByteArray &&buffer);

    QByteArray buffer;

    int width;
    int height;

    QSize size();
    bool isValid();
    bool isNull();

    bool loadFromJpeg(const QByteArray &data);
    bool loadFromPng(const QByteArray &data);

    GreyscaleImage resize(QSize newSize);
    void dither();
    GreyscaleImage rotate(int rotation);
    GreyscaleImage crop(const QRect &rect);

    bool saveAsJpeg(const QString &path);

    QImage toQImage();

private:
    GreyscaleImage rotate_neon(int rot);
    GreyscaleImage transpose_neon();
};

#endif  // GREYSCALEIMAGE_H
