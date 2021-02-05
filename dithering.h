#ifndef DITHERING_H
#define DITHERING_H

#include <QByteArray>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

#ifdef __ARM_NEON__
void dither_NEON(QByteArray &buffer, int width, int height);
#endif

void dither_fallback(QByteArray &buffer, int width, int height);

void dither_auto(QByteArray &buffer, int width, int height);

#endif  // DITHERING_H
