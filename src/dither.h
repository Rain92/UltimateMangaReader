#ifndef DITHER_H
#define DITHER_H

#include <QByteArray>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

void ditherBuffer(QByteArray &buffer, int width, int height);

#endif  // DITHER_H
