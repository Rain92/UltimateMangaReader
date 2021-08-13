#ifndef IMAGEROTATE_H
#define IMAGEROTATE_H

#include "QByteArray"

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

QByteArray rotateBuffer(QByteArray &buffer, int width, int height, int rotation);

#endif  // IMAGEROTATE_H
