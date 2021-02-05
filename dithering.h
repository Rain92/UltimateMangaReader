#ifndef DITHERING_H
#define DITHERING_H

#include <QByteArray>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

static inline uint32_t div255(uint32_t v)
{
    uint32_t _v = v + 128;
    return ((_v >> 8U) + _v) >> 8U;
}

static uint8_t dither_o8x8(unsigned short int x, unsigned short int y, uint8_t v)
{
    // c.f.,
    // https://github.com/ImageMagick/ImageMagick/blob/ecfeac404e75f304004f0566557848c53030bad6/config/thresholds.xml#L107
    static const uint8_t threshold_map_o8x8[] = {
        1,  49, 13, 61, 4,  52, 16, 64, 33, 17, 45, 29, 36, 20, 48, 32, 9,  57, 5,  53, 12, 60,
        8,  56, 41, 25, 37, 21, 44, 28, 40, 24, 3,  51, 15, 63, 2,  50, 14, 62, 35, 19, 47, 31,
        34, 18, 46, 30, 11, 59, 7,  55, 10, 58, 6,  54, 43, 27, 39, 23, 42, 26, 38, 22};

    // Constants:
    // Quantum = 8; Levels = 16; map Divisor = 65
    // QuantumRange = 0xFF
    // QuantumScale = 1.0 / QuantumRange
    //
    // threshold = QuantumScale * v * ((L-1) * (D-1) + 1)
    // NOTE: The initial computation of t (specifically, what we pass to DIV255) would overflow an uint8_t.
    //       With a Q8 input value, we're at no risk of ever underflowing, so, keep to unsigned maths.
    //       Technically, an uint16_t would be wide enough, but it gains us nothing,
    //       and requires a few explicit casts to make GCC happy ;).
    uint32_t t = div255(v * ((15U << 6U) + 1U));
    // level = t / (D-1);
    const uint32_t l = (t >> 6U);
    // t -= l * (D-1);
    t = (t - (l << 6U));

    // map width & height = 8
    // c = ClampToQuantum((l+(t >= map[(x % mw) + mw * (y % mh)])) * QuantumRange / (L-1));
    const uint32_t q = ((l + (t >= threshold_map_o8x8[(x & 7U) + 8U * (y & 7U)])) * 17U);
    // NOTE: We're doing unsigned maths, so, clamping is basically MIN(q, UINT8_MAX) ;).
    //       The only overflow we should ever catch should be for a few white (v = 0xFF) input pixels
    //       that get shifted to the next step (i.e., q = 272 (0xFF + 17)).
    return (q > UINT8_MAX ? UINT8_MAX : (uint8_t)q);
}

#ifdef __ARM_NEON__
static inline uint16x4_t vdiv255(uint32x4_t vec)
{
    uint32x4_t vc128 = vdupq_n_u32(128);
    uint32x4_t vec_v = vaddq_u32(vec, vc128);
    uint32x4_t res = vshrq_n_u32(vaddq_u32(vshrq_n_u32(vec_v, 8), vec_v), 8);
    uint16x4_t res_downcast = vmovn_u32(res);
    return res_downcast;
}

static void dither_NEON(QByteArray &buffer, int width, int height)
{
    static const uint8_t threshold_map_o8x8[] = {
        1,  49, 13, 61, 4,  52, 16, 64, 33, 17, 45, 29, 36, 20, 48, 32, 9,  57, 5,  53, 12, 60,
        8,  56, 41, 25, 37, 21, 44, 28, 40, 24, 3,  51, 15, 63, 2,  50, 14, 62, 35, 19, 47, 31,
        34, 18, 46, 30, 11, 59, 7,  55, 10, 58, 6,  54, 43, 27, 39, 23, 42, 26, 38, 22};

    int x = 0, y = 0, cp = 0;

    if (width >= 8)
    {
        uint16_t cx = ((15U << 6) + 1U);

        uint16x8_t vc1 = vdupq_n_u16(1);
        uint16x8_t vc255 = vdupq_n_u16(255);

        uint16x4_t vcx = vdup_n_u16(cx);

        uint8x8_t vecn = vld1_u8((uchar *)buffer.data());

        int line_leftover = (8 - width) & 7;
        for (y = 0, cp = 0; y < height; y++, cp -= line_leftover)
            for (x = 0; x < width && cp + 8 <= buffer.size(); x += 8, cp += 8)
            {
                // uint8x8_t vecn = vld1_u8((uchar*)buffer.data() + cp);
                uint16x8_t vec = vmovl_u8(vecn);
                uint16x4_t vec_1 = vget_low_u16(vec);
                uint16x4_t vec_2 = vget_high_u16(vec);

                uint32x4_t vect0_1 = vmull_u16(vec_1, vcx);
                uint32x4_t vect0_2 = vmull_u16(vec_2, vcx);

                uint16x4_t vect_1 = vdiv255(vect0_1);
                uint16x4_t vect_2 = vdiv255(vect0_2);

                uint16x8_t vect = vcombine_u16(vect_1, vect_2);

                uint16x8_t vecl = vshrq_n_u16(vect, 6);
                vect = vsubq_u16(vect, vshlq_n_u16(vecl, 6));

                uint8x8_t vecthreshn = vld1_u8(&threshold_map_o8x8[(x & 7U) + 8U * (y & 7U)]);
                uint16x8_t vecthresh = vmovl_u8(vecthreshn);

                uint16x8_t vecm = vcgeq_u16(vect, vecthresh);

                uint16x8_t vecl2 = vaddq_u16(vecl, vc1);

                uint16x8_t vecq = vbslq_u16(vecm, vecl2, vecl);
                vecq = vmulq_n_u16(vecq, 17);
                vecq = vminq_u16(vecq, vc255);

                uint8x8_t vecqb = vmovn_u16(vecq);

                // load next chunk before writing back
                if (cp + 16 <= buffer.size())
                {
                    int offset = cp + 8;
                    if (x + 8 >= width)
                        offset -= line_leftover;
                    vecn = vld1_u8((uchar *)buffer.data() + offset);
                }

                vst1_u8((uchar *)buffer.data() + cp, vecqb);
            }

        // wind back increments
        cp -= 8 - line_leftover;
        x -= 8;
        y -= 1;
    }

    // take care of leftovers
    for (; y < height; y++)
        for (; x < width; x++, cp++)
            buffer[cp] = dither_o8x8(x, y, buffer[cp]);
}
#endif

static void dither_fallback(QByteArray &buffer, int width, int height)
{
    for (int y = 0, p = 0; y < height; y++)
        for (int x = 0; x < width; x++, p++)
            buffer[p] = dither_o8x8(x, y, buffer[p]);
}

static void dither_auto(QByteArray &buffer, int width, int height)
{
#ifdef __ARM_NEON__
    dither_NEON(buffer, width, height);
#else
    dither_fallback(buffer, width, height);
#endif
}

#endif  // DITHERING_H
