#pragma once

#include <QByteArray>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

// Rotates an 8 bit grayscale image by 90, 180 or 270 degrees clockwise.
QByteArray rotateBuffer(const QByteArray &buffer, int width, int height, int rotation);
