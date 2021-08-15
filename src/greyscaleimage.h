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

#include "imagerotate.h"
#include "thirdparty/simdimageresize.h"

bool isJpeg(const QByteArray &buffer);
bool isPng(const QByteArray &buffer);

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

    bool loadFromEncoded(const QByteArray &data);
    bool loadFromJpeg(const QByteArray &data);
    bool loadFromPng(const QByteArray &data);

    GreyscaleImage resize(QSize newSize);
    GreyscaleImage rotate(int rotation);
    GreyscaleImage crop(QRect rect);

    bool saveAsJpeg(const QString &path);

    QImage toQImage();

private:
    GreyscaleImage rotate_neon(int rot);
    GreyscaleImage transpose_neon();
};

#endif  // GREYSCALEIMAGE_H
