#include "imagerotate.h"

#include <algorithm>
#include <cstdint>

#ifdef __ARM_NEON__

// Transposes a 16x16 byte block held in 16 vector registers (row i in r[i]).
// Classic 4 stage butterfly: interleave pairs at 8, 16, 32 and 64 bit granularity.
static inline void transpose16x16(uint8x16_t r[16])
{
    for (int i = 0; i < 16; i += 2)
    {
        uint8x16x2_t t = vtrnq_u8(r[i], r[i + 1]);
        r[i] = t.val[0];
        r[i + 1] = t.val[1];
    }
    for (int i = 0; i < 16; i += 4)
        for (int j = 0; j < 2; j++)
        {
            uint16x8x2_t t = vtrnq_u16(vreinterpretq_u16_u8(r[i + j]), vreinterpretq_u16_u8(r[i + j + 2]));
            r[i + j] = vreinterpretq_u8_u16(t.val[0]);
            r[i + j + 2] = vreinterpretq_u8_u16(t.val[1]);
        }
    for (int i = 0; i < 16; i += 8)
        for (int j = 0; j < 4; j++)
        {
            uint32x4x2_t t = vtrnq_u32(vreinterpretq_u32_u8(r[i + j]), vreinterpretq_u32_u8(r[i + j + 4]));
            r[i + j] = vreinterpretq_u8_u32(t.val[0]);
            r[i + j + 4] = vreinterpretq_u8_u32(t.val[1]);
        }
    for (int j = 0; j < 8; j++)
    {
        uint8x16_t a = r[j], b = r[j + 8];
        r[j] = vcombine_u8(vget_low_u8(a), vget_low_u8(b));
        r[j + 8] = vcombine_u8(vget_high_u8(a), vget_high_u8(b));
    }
}

// The 16x16 blocks are visited in strips of STRIP source rows: for each block column all blocks of the
// strip are transposed before moving on. That way every destination row receives STRIP consecutive
// bytes (full cache lines) while the strip's source lines (STRIP lines) stay in L1.
static const int STRIP = 64;

// 90 degrees clockwise: dst[x][h - 1 - y] = src[y][x], dst is h wide
static void rotate90_NEON(const uint8_t *src, int width, int height, uint8_t *dst)
{
    const int nw = height;
    const int sw = width & ~15, sh = height & ~15;

    for (int ys = 0; ys < sh; ys += STRIP)
    {
        const int ye = std::min(ys + STRIP, sh);
        for (int x0 = 0; x0 < sw; x0 += 16)
            for (int y0 = ys; y0 < ye; y0 += 16)
            {
                uint8x16_t r[16];
                // rows loaded bottom up, so the transposed rows come out already reversed
                const uint8_t *s = src + (y0 + 15) * width + x0;
                for (int j = 0; j < 16; j++, s -= width)
                {
                    __builtin_prefetch(s + 64);
                    r[j] = vld1q_u8(s);
                }
                transpose16x16(r);
                uint8_t *d = dst + x0 * nw + (nw - 16 - y0);
                for (int i = 0; i < 16; i++, d += nw)
                    vst1q_u8(d, r[i]);
            }
    }

    for (int y = 0; y < height; y++)
        for (int x = sw; x < width; x++)
            dst[x * nw + (nw - 1 - y)] = src[y * width + x];
    for (int y = sh; y < height; y++)
        for (int x = 0; x < sw; x++)
            dst[x * nw + (nw - 1 - y)] = src[y * width + x];
}

// 270 degrees clockwise (90 ccw): dst[w - 1 - x][y] = src[y][x], dst is h wide
static void rotate270_NEON(const uint8_t *src, int width, int height, uint8_t *dst)
{
    const int nw = height;
    const int sw = width & ~15, sh = height & ~15;

    for (int ys = 0; ys < sh; ys += STRIP)
    {
        const int ye = std::min(ys + STRIP, sh);
        for (int x0 = 0; x0 < sw; x0 += 16)
            for (int y0 = ys; y0 < ye; y0 += 16)
            {
                uint8x16_t r[16];
                const uint8_t *s = src + y0 * width + x0;
                for (int j = 0; j < 16; j++, s += width)
                {
                    __builtin_prefetch(s + 64);
                    r[j] = vld1q_u8(s);
                }
                transpose16x16(r);
                uint8_t *d = dst + (width - 1 - x0) * nw + y0;
                for (int i = 0; i < 16; i++, d -= nw)
                    vst1q_u8(d, r[i]);
            }
    }

    for (int y = 0; y < height; y++)
        for (int x = sw; x < width; x++)
            dst[(width - 1 - x) * nw + y] = src[y * width + x];
    for (int y = sh; y < height; y++)
        for (int x = 0; x < sw; x++)
            dst[(width - 1 - x) * nw + y] = src[y * width + x];
}

// 180 degrees: dst[size - 1 - i] = src[i]
static void rotate180_NEON(const uint8_t *src, int size, uint8_t *dst)
{
    int i = 0;
    for (; i + 16 <= size; i += 16)
    {
        __builtin_prefetch(src + i + 128);
        uint8x16_t v = vrev64q_u8(vld1q_u8(src + i));
        v = vcombine_u8(vget_high_u8(v), vget_low_u8(v));
        vst1q_u8(dst + size - 16 - i, v);
    }
    for (; i < size; i++)
        dst[size - 1 - i] = src[i];
}

static void rotateBuffer_NEON(const uint8_t *src, int width, int height, int rotation, uint8_t *dst)
{
    if (rotation == 90)
        rotate90_NEON(src, width, height, dst);
    else if (rotation == 270)
        rotate270_NEON(src, width, height, dst);
    else if (rotation == 180)
        rotate180_NEON(src, width * height, dst);
}

#else

static void rotateBuffer_fallback(const uint8_t *src, int width, int height, int rotation, uint8_t *dst)
{
    int nw = rotation == 180 ? width : height;
    int nh = rotation == 180 ? height : width;

    if (rotation == 90)
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++)
                dst[y * nw + x] = src[(height - 1 - x) * width + y];

    else if (rotation == 270)
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++)
                dst[y * nw + x] = src[x * width + (width - 1 - y)];

    else if (rotation == 180)
        for (int p1 = 0, p2 = width * height - 1; p2 >= 0; p1++, p2--)
            dst[p2] = src[p1];
}

#endif

QByteArray rotateBuffer(const QByteArray &buffer, int width, int height, int rotation)
{
    if (rotation % 90 != 0)
        return QByteArray();

    if (rotation == 0)
        return QByteArray(buffer);

    QByteArray newBuffer;
    newBuffer.resize(width * height);

    const uint8_t *src = reinterpret_cast<const uint8_t *>(buffer.constData());
    uint8_t *dst = reinterpret_cast<uint8_t *>(newBuffer.data());

#ifdef __ARM_NEON__
    rotateBuffer_NEON(src, width, height, rotation, dst);
#else
    rotateBuffer_fallback(src, width, height, rotation, dst);
#endif

    return newBuffer;
}
