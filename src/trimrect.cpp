#include "trimrect.h"

#include <QtGlobal>
#include <utility>

#ifdef __ARM_NEON__
#include <arm_neon.h>

// Packs a 16 lane byte mask into 64 bits, 4 bits per lane (lane i -> bits 4i..4i+3).
static inline uint64_t packMask16(uint8x16_t mask)
{
    uint8x8_t packed = vshrn_n_u16(vreinterpretq_u16_u8(mask), 4);
    return vget_lane_u64(vreinterpret_u64_u8(packed), 0);
}

// first index in [0, limit) with linePtr[index] <= threshold, or limit
static inline int findFirstDark(const uchar *linePtr, int limit, const uchar threshold)
{
    const uint8x16_t vthreshold = vdupq_n_u8(threshold);
    int i = 0;
    for (; i + 16 <= limit; i += 16)
    {
        uint64_t bits = packMask16(vcleq_u8(vld1q_u8(linePtr + i), vthreshold));
        if (bits)
            return i + (__builtin_ctzll(bits) >> 2);
    }
    for (; i < limit; i++)
        if (linePtr[i] <= threshold)
            return i;
    return limit;
}

// last index in [limit, imgWidth) with linePtr[index] <= threshold, or limit - 1
static inline int findLastDark(const uchar *linePtr, int imgWidth, int limit, const uchar threshold)
{
    const uint8x16_t vthreshold = vdupq_n_u8(threshold);
    int i = imgWidth;
    for (; i - 16 >= limit; i -= 16)
    {
        uint64_t bits = packMask16(vcleq_u8(vld1q_u8(linePtr + i - 16), vthreshold));
        if (bits)
            return i - 16 + ((63 - __builtin_clzll(bits)) >> 2);
    }
    for (i--; i >= limit; i--)
        if (linePtr[i] <= threshold)
            return i;
    return limit - 1;
}
#else
static inline int findFirstDark(const uchar *linePtr, int limit, const uchar threshold)
{
    int i = 0;
    for (; i < limit; i++)
        if (linePtr[i] <= threshold)
            break;
    return i;
}

static inline int findLastDark(const uchar *linePtr, int imgWidth, int limit, const uchar threshold)
{
    int i = imgWidth - 1;
    for (; i >= limit; i--)
        if (linePtr[i] <= threshold)
            break;
    return i;
}
#endif

static inline std::pair<int, int> getTrimRectHelper(const uchar *linePtr, int imgWidth, int limitLeft,
                                                    int limitRight, const uchar threshold)
{
    int left = findFirstDark(linePtr, limitLeft, threshold);
    int right = findLastDark(linePtr, imgWidth, limitRight, threshold);

    return {left, right};
}

QRect getTrimRect(const QByteArray &buffer, int imgWidth, int imgHeight, int stride)
{
    const uchar threshold = 234;

    int bottom = 0;
    int top = 0;
    int leftMin = imgWidth;
    int rightMin = 0;

    uchar *linePtr;

    for (top = 0; top < imgHeight; top++)
    {
        linePtr = (uchar *)buffer.data() + top * stride;
        auto [left, right] = getTrimRectHelper(linePtr, imgWidth, imgWidth, rightMin, threshold);

        bool allWhite = left == imgWidth;
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
            break;
        }
    }

    for (bottom = imgHeight - 1; bottom >= top; bottom--)
    {
        linePtr = (uchar *)buffer.data() + bottom * stride;
        auto [left, right] = getTrimRectHelper(linePtr, imgWidth, imgWidth, rightMin, threshold);

        bool allWhite = left == imgWidth;
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
            break;
        }
    }

    for (int middle = top; middle < bottom; middle++)
    {
        linePtr = (uchar *)buffer.data() + middle * stride;
        auto [left, right] = getTrimRectHelper(linePtr, imgWidth, imgWidth, rightMin, threshold);

        bool allWhite = left == imgWidth;
        if (!allWhite)
        {
            leftMin = qMin(leftMin, left);
            rightMin = qMax(rightMin, right);
        }
    }

    int width = rightMin - leftMin + 1;
    int height = bottom - top + 1;
    int ntw = qMax(0, width + 4 - width % 4);
    if (ntw + rightMin > imgWidth)
        ntw -= 4;

    return QRect(leftMin, top, ntw, height);
}
