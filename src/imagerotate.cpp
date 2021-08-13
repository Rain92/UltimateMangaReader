#include "imagerotate.h"

#ifdef __ARM_NEON__
void rotateBuffer_NEON(QByteArray &buffer, int width, int height, int rotation, QByteArray &newBuffer)
{
    int nw = rotation == 180 ? width : height;
    int nh = rotation == 180 ? height : width;

    if (rotation == 90)
    {
        const int blocksize = 8;
        uint8x8_t inblock[blocksize];
        uint16x4_t trans2[blocksize];
        uint32x2_t trans4[blocksize];
        uint32x2_t trans8[blocksize];

        int sw = width - (width & (blocksize - 1));
        int sh = height - (height & (blocksize - 1));
        int x, y;
        for (y = 0; y < sh; y += blocksize)
            for (x = 0; x < sw; x += blocksize)
            {
#pragma GCC unroll 8
                for (int i = 0; i < blocksize; i++)
                    inblock[i] = vld1_u8((uint8_t *)buffer.data() + (y + i) * width + x);

                //#pragma GCC unroll 4
                for (int i = 0; i < blocksize; i += 2)
                {
                    auto t = vtrn_u8(inblock[i + 0], inblock[i + 1]);
                    trans2[i + 0] = vreinterpret_u16_u8(t.val[0]);
                    trans2[i + 1] = vreinterpret_u16_u8(t.val[1]);
                }

                //#pragma GCC unroll 2
                for (int i = 0; i < blocksize; i += 4)
                {
                    auto t1 = vtrn_u16(trans2[i + 0], trans2[i + 2]);
                    auto t2 = vtrn_u16(trans2[i + 1], trans2[i + 3]);

                    trans4[i + 0] = vreinterpret_u32_u16(t1.val[0]);
                    trans4[i + 1] = vreinterpret_u32_u16(t2.val[0]);
                    trans4[i + 2] = vreinterpret_u32_u16(t1.val[1]);
                    trans4[i + 3] = vreinterpret_u32_u16(t2.val[1]);
                }

                {
                    auto t1 = vtrn_u32(trans4[0], trans4[4]);
                    auto t2 = vtrn_u32(trans4[1], trans4[5]);
                    auto t3 = vtrn_u32(trans4[2], trans4[6]);
                    auto t4 = vtrn_u32(trans4[3], trans4[7]);
                    trans8[0] = t1.val[0];
                    trans8[1] = t2.val[0];
                    trans8[2] = t3.val[0];
                    trans8[3] = t4.val[0];
                    trans8[4] = t1.val[1];
                    trans8[5] = t2.val[1];
                    trans8[6] = t3.val[1];
                    trans8[7] = t4.val[1];
                }

#pragma GCC unroll 8
                for (int i = 0; i < blocksize; i++)
                {
                    auto vrev = vrev64_u8(vreinterpret_u8_u32(trans8[i]));
                    vst1_u8((uint8_t *)newBuffer.data() + (x + i) * nw + (nw - y - 8), vrev);
                }
            }

        for (y = sh - 1; y < height; y++)
            for (x = 0; x < width; x++)
                newBuffer[x * nw + (nw - y - 1)] = buffer[y * width + x];
        for (x = sw - 1; x < width; x++)
            for (y = 0; y < height; y++)
                newBuffer[x * nw + (nw - y - 1)] = buffer[y * width + x];
    }

    else if (rotation == 270)
    {
        const int blocksize = 8;
        uint8x8_t inblock[blocksize];
        uint16x4_t trans2[blocksize];
        uint32x2_t trans4[blocksize];
        uint32x2_t trans8[blocksize];

        int sw = width - (width & (blocksize - 1));
        int sh = height - (height & (blocksize - 1));
        int x, y;
        for (y = 0; y < sh; y += blocksize)
            for (x = 0; x < sw; x += blocksize)
            {
#pragma GCC unroll 8
                for (int i = 0; i < blocksize; i++)
                    inblock[i] = vld1_u8((uint8_t *)buffer.data() + (y + i) * width + x);

                //#pragma GCC unroll 4
                for (int i = 0; i < blocksize; i += 2)
                {
                    auto t = vtrn_u8(inblock[i + 0], inblock[i + 1]);
                    trans2[i + 0] = vreinterpret_u16_u8(t.val[0]);
                    trans2[i + 1] = vreinterpret_u16_u8(t.val[1]);
                }

                //#pragma GCC unroll 2
                for (int i = 0; i < blocksize; i += 4)
                {
                    auto t1 = vtrn_u16(trans2[i + 0], trans2[i + 2]);
                    auto t2 = vtrn_u16(trans2[i + 1], trans2[i + 3]);

                    trans4[i + 0] = vreinterpret_u32_u16(t1.val[0]);
                    trans4[i + 1] = vreinterpret_u32_u16(t2.val[0]);
                    trans4[i + 2] = vreinterpret_u32_u16(t1.val[1]);
                    trans4[i + 3] = vreinterpret_u32_u16(t2.val[1]);
                }

                {
                    auto t1 = vtrn_u32(trans4[0], trans4[4]);
                    auto t2 = vtrn_u32(trans4[1], trans4[5]);
                    auto t3 = vtrn_u32(trans4[2], trans4[6]);
                    auto t4 = vtrn_u32(trans4[3], trans4[7]);
                    trans8[0] = t1.val[0];
                    trans8[1] = t2.val[0];
                    trans8[2] = t3.val[0];
                    trans8[3] = t4.val[0];
                    trans8[4] = t1.val[1];
                    trans8[5] = t2.val[1];
                    trans8[6] = t3.val[1];
                    trans8[7] = t4.val[1];
                }

#pragma GCC unroll 8
                for (int i = 0; i < blocksize; i++)
                {
                    vst1_u8((uint8_t *)newBuffer.data() + (nh - 8 - x + (7 - i)) * nw + y,
                            vreinterpret_u8_u32(trans8[i]));
                }
            }

        for (y = sh - 1; y < height; y++)
            for (x = 0; x < width; x++)
                newBuffer[(nh - 1 - x) * nw + y] = buffer[y * width + x];
        for (x = sw - 1; x < width; x++)
            for (y = 0; y < height; y++)
                newBuffer[(nh - 1 - x) * nw + y] = buffer[y * width + x];
    }
    else if (rotation == 180)
    {
        int p1, p2;
        for (p1 = 0, p2 = width * height - 16; p2 >= 0; p1 += 16, p2 -= 16)
        {
            __builtin_prefetch(buffer.data() + p1 + 384);
            uint8x16_t v = vld1q_u8((uint8_t *)buffer.data() + p1);
            uint8x16_t vrev = vrev64q_u8(v);
            vrev = vcombine_u8(vget_high_u8(vrev), vget_low_u8(vrev));
            vst1q_u8((uint8_t *)newBuffer.data() + p2, vrev);
        }

        for (p1 -= 16, p2 += 16; p2 >= 0; p1++, p2--)
            newBuffer[p2] = buffer[p1];
    }
}

void transposeBuffer_NEON(QByteArray &buffer, int width, int height, QByteArray &newBuffer)
{
    int nw = height;
    int nh = width;

    const int blocksize = 8;
    uint8x8_t inblock[blocksize];
    uint16x4_t trans2[blocksize];
    uint32x2_t trans4[blocksize];
    uint32x2_t trans8[blocksize];

    int sw = width - (width & (blocksize - 1));
    int sh = height - (height & (blocksize - 1));
    int x, y;
    for (y = 0; y < sh; y += blocksize)
        for (x = 0; x < sw; x += blocksize)
        {
#pragma GCC unroll 8
            for (int i = 0; i < blocksize; i++)
                inblock[i] = vld1_u8((uint8_t *)buffer.data() + (y + i) * width + x);

            //#pragma GCC unroll 4
            for (int i = 0; i < blocksize; i += 2)
            {
                auto t = vtrn_u8(inblock[i + 0], inblock[i + 1]);
                trans2[i + 0] = vreinterpret_u16_u8(t.val[0]);
                trans2[i + 1] = vreinterpret_u16_u8(t.val[1]);
            }

            //#pragma GCC unroll 2
            for (int i = 0; i < blocksize; i += 4)
            {
                auto t1 = vtrn_u16(trans2[i + 0], trans2[i + 2]);
                auto t2 = vtrn_u16(trans2[i + 1], trans2[i + 3]);

                trans4[i + 0] = vreinterpret_u32_u16(t1.val[0]);
                trans4[i + 1] = vreinterpret_u32_u16(t2.val[0]);
                trans4[i + 2] = vreinterpret_u32_u16(t1.val[1]);
                trans4[i + 3] = vreinterpret_u32_u16(t2.val[1]);
            }

            {
                auto t1 = vtrn_u32(trans4[0], trans4[4]);
                auto t2 = vtrn_u32(trans4[1], trans4[5]);
                auto t3 = vtrn_u32(trans4[2], trans4[6]);
                auto t4 = vtrn_u32(trans4[3], trans4[7]);
                trans8[0] = t1.val[0];
                trans8[1] = t2.val[0];
                trans8[2] = t3.val[0];
                trans8[3] = t4.val[0];
                trans8[4] = t1.val[1];
                trans8[5] = t2.val[1];
                trans8[6] = t3.val[1];
                trans8[7] = t4.val[1];
            }

#pragma GCC unroll 8
            for (int i = 0; i < blocksize; i++)
                vst1_u8((uint8_t *)newBuffer.data() + (x + i) * nw + y, vreinterpret_u8_u32(trans8[i]));
        }
    for (y = sh - 1; y < height; y++)
        for (x = 0; x < width; x++)
            newBuffer[x * nw + y] = buffer[y * width + x];
    for (x = sw - 1; x < width; x++)
        for (y = 0; y < height; y++)
            newBuffer[x * nw + y] = buffer[y * width + x];
}

#endif

void rotateBuffer_fallback(QByteArray &buffer, int width, int height, int rotation, QByteArray &newBuffer)
{
    int nw = rotation == 180 ? width : height;
    int nh = rotation == 180 ? height : width;

    if (rotation == 90)
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++)
                newBuffer[y * nw + x] = buffer[(height - 1 - x) * width + y];

    else if (rotation == 270)
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++)
                newBuffer[y * nw + x] = buffer[x * width + (width - 1 - y)];

    else if (rotation == 180)
        for (int p1 = 0, p2 = width * height - 1; p2 >= 0; p1++, p2--)
            newBuffer[p2] = buffer[p1];
}

QByteArray rotateBuffer(QByteArray &buffer, int width, int height, int rotation)
{
    if (rotation % 90 != 0)
        return QByteArray();

    if (rotation == 0)
        return QByteArray(buffer);

    QByteArray newBuffer;
    newBuffer.resize(width * height);

#ifdef __ARM_NEON__
    rotateBuffer_NEON(buffer, width, height, rotation, newBuffer);
#else
    rotateBuffer_fallback(buffer, width, height, rotation, newBuffer);
#endif

    return newBuffer;
}
