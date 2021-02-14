/*
 * Simd Library (http://ermig1979.github.io/Simd).
 *
 * Copyright (c) 2011-2020 Yermalayeu Ihar,
 *               2014-2015 Antonenka Mikhail.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SimdBase_h__
#define __SimdBase_h__

#ifdef __ARM_NEON__
#include <arm_neon.h>
#define SIMD_NEON_PREFECH_SIZE 384
#define SIMD_NEON_ENABLE
#endif

#include <assert.h>
#include <limits.h>
#include <memory.h>

#include <cstdint>
#include <cstdlib>

#define SIMD_INLINE inline
#define SIMD_ALIGN 16
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

namespace Simd
{
#define SIMD_CHAR_AS_LONGLONG(a) (((long long)a) & 0xFF)

#define SIMD_SHORT_AS_LONGLONG(a) (((long long)a) & 0xFFFF)

#define SIMD_INT_AS_LONGLONG(a) (((long long)a) & 0xFFFFFFFF)

#define SIMD_LL_SET1_EPI8(a)                                                                        \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(a) << 8) | (SIMD_CHAR_AS_LONGLONG(a) << 16) | \
        (SIMD_CHAR_AS_LONGLONG(a) << 24) | (SIMD_CHAR_AS_LONGLONG(a) << 32) |                       \
        (SIMD_CHAR_AS_LONGLONG(a) << 40) | (SIMD_CHAR_AS_LONGLONG(a) << 48) |                       \
        (SIMD_CHAR_AS_LONGLONG(a) << 56)

#define SIMD_LL_SET2_EPI8(a, b)                                                                     \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(b) << 8) | (SIMD_CHAR_AS_LONGLONG(a) << 16) | \
        (SIMD_CHAR_AS_LONGLONG(b) << 24) | (SIMD_CHAR_AS_LONGLONG(a) << 32) |                       \
        (SIMD_CHAR_AS_LONGLONG(b) << 40) | (SIMD_CHAR_AS_LONGLONG(a) << 48) |                       \
        (SIMD_CHAR_AS_LONGLONG(b) << 56)

#define SIMD_LL_SETR_EPI8(a, b, c, d, e, f, g, h)                                                   \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(b) << 8) | (SIMD_CHAR_AS_LONGLONG(c) << 16) | \
        (SIMD_CHAR_AS_LONGLONG(d) << 24) | (SIMD_CHAR_AS_LONGLONG(e) << 32) |                       \
        (SIMD_CHAR_AS_LONGLONG(f) << 40) | (SIMD_CHAR_AS_LONGLONG(g) << 48) |                       \
        (SIMD_CHAR_AS_LONGLONG(h) << 56)

#define SIMD_LL_SET1_EPI16(a)                                                                           \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(a) << 16) | (SIMD_SHORT_AS_LONGLONG(a) << 32) | \
        (SIMD_SHORT_AS_LONGLONG(a) << 48)

#define SIMD_LL_SET2_EPI16(a, b)                                                                        \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(b) << 16) | (SIMD_SHORT_AS_LONGLONG(a) << 32) | \
        (SIMD_SHORT_AS_LONGLONG(b) << 48)

#define SIMD_LL_SETR_EPI16(a, b, c, d)                                                                  \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(b) << 16) | (SIMD_SHORT_AS_LONGLONG(c) << 32) | \
        (SIMD_SHORT_AS_LONGLONG(d) << 48)

#define SIMD_LL_SET1_EPI32(a) SIMD_INT_AS_LONGLONG(a) | (SIMD_INT_AS_LONGLONG(a) << 32)

#define SIMD_LL_SET2_EPI32(a, b) SIMD_INT_AS_LONGLONG(a) | (SIMD_INT_AS_LONGLONG(b) << 32)

#define SIMD_VEC_SET1_EPI8(a)                          \
    {                                                  \
        a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a \
    }

#define SIMD_VEC_SET2_EPI8(a0, a1)                                     \
    {                                                                  \
        a0, a1, a0, a1, a0, a1, a0, a1, a0, a1, a0, a1, a0, a1, a0, a1 \
    }

#define SIMD_VEC_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {                                                                                      \
        a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af                     \
    }

#define SIMD_VEC_SET1_EPI16(a) \
    {                          \
        a, a, a, a, a, a, a, a \
    }

#define SIMD_VEC_SET2_EPI16(a0, a1)    \
    {                                  \
        a0, a1, a0, a1, a0, a1, a0, a1 \
    }

#define SIMD_VEC_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7) \
    {                                                       \
        a0, a1, a2, a3, a4, a5, a6, a7                      \
    }

#define SIMD_VEC_SET1_EPI32(a) \
    {                          \
        a, a, a, a             \
    }

#define SIMD_VEC_SET2_EPI32(a0, a1) \
    {                               \
        a0, a1, a0, a1              \
    }

#define SIMD_VEC_SETR_EPI32(a0, a1, a2, a3) \
    {                                       \
        a0, a1, a2, a3                      \
    }

#define SIMD_VEC_SET1_EPI64(a) \
    {                          \
        a, a                   \
    }

#define SIMD_VEC_SET2_EPI64(a0, a1) \
    {                               \
        a0, a1                      \
    }

#define SIMD_VEC_SETR_EPI64(a0, a1) \
    {                               \
        a0, a1                      \
    }

#define SIMD_VEC_SET1_PS(a) \
    {                       \
        a, a, a, a          \
    }

#define SIMD_VEC_SET2_PS(a0, a1) \
    {                            \
        a0, a1, a0, a1           \
    }

#define SIMD_VEC_SETR_PS(a0, a1, a2, a3) \
    {                                    \
        a0, a1, a2, a3                   \
    }

#define SIMD_VEC_SET1_PI8(a)   \
    {                          \
        a, a, a, a, a, a, a, a \
    }

#define SIMD_VEC_SET2_PI8(a0, a1)      \
    {                                  \
        a0, a1, a0, a1, a0, a1, a0, a1 \
    }

#define SIMD_VEC_SETR_PI8(a0, a1, a2, a3, a4, a5, a6, a7) \
    {                                                     \
        a0, a1, a2, a3, a4, a5, a6, a7                    \
    }

#define SIMD_VEC_SET1_PI16(a) \
    {                         \
        a, a, a, a            \
    }

#define SIMD_VEC_SET2_PI16(a0, a1) \
    {                              \
        a0, a1, a0, a1             \
    }

#define SIMD_VEC_SETR_PI16(a0, a1, a2, a3) \
    {                                      \
        a0, a1, a2, a3                     \
    }

#define SIMD_VEC_SET1_PI32(a) \
    {                         \
        a, a                  \
    }

#define SIMD_VEC_SETR_PI32(a0, a1) \
    {                              \
        a0, a1                     \
    }

#define SIMD_VEC_SETR_PI64(a) \
    {                         \
        a                     \
    }

const size_t HISTOGRAM_SIZE = UCHAR_MAX + 1;

namespace Base
{
const int LINEAR_SHIFT = 4;
const int LINEAR_ROUND_TERM = 1 << (LINEAR_SHIFT - 1);

const int BILINEAR_SHIFT = LINEAR_SHIFT * 2;
const int BILINEAR_ROUND_TERM = 1 << (BILINEAR_SHIFT - 1);

const int FRACTION_RANGE = 1 << LINEAR_SHIFT;
const double FRACTION_ROUND_TERM = 0.5 / FRACTION_RANGE;

const float KF_255_DIV_6 = 255.0f / 6.0f;

const int BGR_TO_GRAY_AVERAGING_SHIFT = 14;
const int BGR_TO_GRAY_ROUND_TERM = 1 << (BGR_TO_GRAY_AVERAGING_SHIFT - 1);
const int BLUE_TO_GRAY_WEIGHT = int(0.114 * (1 << BGR_TO_GRAY_AVERAGING_SHIFT) + 0.5);
const int GREEN_TO_GRAY_WEIGHT = int(0.587 * (1 << BGR_TO_GRAY_AVERAGING_SHIFT) + 0.5);
const int RED_TO_GRAY_WEIGHT = int(0.299 * (1 << BGR_TO_GRAY_AVERAGING_SHIFT) + 0.5);

const int Y_ADJUST = 16;
const int UV_ADJUST = 128;
const int YUV_TO_BGR_AVERAGING_SHIFT = 13;
const int YUV_TO_BGR_ROUND_TERM = 1 << (YUV_TO_BGR_AVERAGING_SHIFT - 1);
const int Y_TO_RGB_WEIGHT = int(1.164 * (1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
const int U_TO_BLUE_WEIGHT = int(2.018 * (1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
const int U_TO_GREEN_WEIGHT = -int(0.391 * (1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
const int V_TO_GREEN_WEIGHT = -int(0.813 * (1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
const int V_TO_RED_WEIGHT = int(1.596 * (1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);

const int BGR_TO_YUV_AVERAGING_SHIFT = 14;
const int BGR_TO_YUV_ROUND_TERM = 1 << (BGR_TO_YUV_AVERAGING_SHIFT - 1);
const int BLUE_TO_Y_WEIGHT = int(0.098 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int GREEN_TO_Y_WEIGHT = int(0.504 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int RED_TO_Y_WEIGHT = int(0.257 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int BLUE_TO_U_WEIGHT = int(0.439 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int GREEN_TO_U_WEIGHT = -int(0.291 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int RED_TO_U_WEIGHT = -int(0.148 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int BLUE_TO_V_WEIGHT = -int(0.071 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int GREEN_TO_V_WEIGHT = -int(0.368 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
const int RED_TO_V_WEIGHT = int(0.439 * (1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);

const int DIVISION_BY_9_SHIFT = 16;
const int DIVISION_BY_9_FACTOR = (1 << DIVISION_BY_9_SHIFT) / 9;
}  // namespace Base

#ifdef SIMD_NEON_ENABLE
namespace Neon
{
const size_t A = sizeof(uint8x16_t);
const size_t DA = 2 * A;
const size_t QA = 4 * A;
const size_t OA = 8 * A;
const size_t HA = A / 2;

const size_t F = sizeof(float32x4_t) / sizeof(float);
const size_t DF = 2 * F;
const size_t QF = 4 * F;
const size_t HF = F / 2;

const uint8x16_t K8_00 = SIMD_VEC_SET1_EPI8(0x00);
const uint8x16_t K8_01 = SIMD_VEC_SET1_EPI8(0x01);
const uint8x16_t K8_02 = SIMD_VEC_SET1_EPI8(0x02);
const uint8x16_t K8_03 = SIMD_VEC_SET1_EPI8(0x03);
const uint8x16_t K8_04 = SIMD_VEC_SET1_EPI8(0x04);
const uint8x16_t K8_07 = SIMD_VEC_SET1_EPI8(0x07);
const uint8x16_t K8_08 = SIMD_VEC_SET1_EPI8(0x08);
const uint8x16_t K8_10 = SIMD_VEC_SET1_EPI8(0x10);
const uint8x16_t K8_20 = SIMD_VEC_SET1_EPI8(0x20);
const uint8x16_t K8_40 = SIMD_VEC_SET1_EPI8(0x40);
const uint8x16_t K8_80 = SIMD_VEC_SET1_EPI8(0x80);
const uint8x16_t K8_FF = SIMD_VEC_SET1_EPI8(0xFF);

const uint16x8_t K16_0000 = SIMD_VEC_SET1_EPI16(0x0000);
const uint16x8_t K16_0001 = SIMD_VEC_SET1_EPI16(0x0001);
const uint16x8_t K16_0002 = SIMD_VEC_SET1_EPI16(0x0002);
const uint16x8_t K16_0003 = SIMD_VEC_SET1_EPI16(0x0003);
const uint16x8_t K16_0004 = SIMD_VEC_SET1_EPI16(0x0004);
const uint16x8_t K16_0005 = SIMD_VEC_SET1_EPI16(0x0005);
const uint16x8_t K16_0006 = SIMD_VEC_SET1_EPI16(0x0006);
const uint16x8_t K16_0008 = SIMD_VEC_SET1_EPI16(0x0008);
const uint16x8_t K16_0010 = SIMD_VEC_SET1_EPI16(0x0010);
const uint16x8_t K16_0020 = SIMD_VEC_SET1_EPI16(0x0020);
const uint16x8_t K16_0080 = SIMD_VEC_SET1_EPI16(0x0080);
const uint16x8_t K16_00FF = SIMD_VEC_SET1_EPI16(0x00FF);
const uint16x8_t K16_0101 = SIMD_VEC_SET1_EPI16(0x0101);
const uint16x8_t K16_0800 = SIMD_VEC_SET1_EPI16(0x0800);
const uint16x8_t K16_FF00 = SIMD_VEC_SET1_EPI16(0xFF00);

const uint32x4_t K32_00000000 = SIMD_VEC_SET1_EPI32(0x00000000);
const uint32x4_t K32_00000001 = SIMD_VEC_SET1_EPI32(0x00000001);
const uint32x4_t K32_00000002 = SIMD_VEC_SET1_EPI32(0x00000002);
const uint32x4_t K32_00000003 = SIMD_VEC_SET1_EPI32(0x00000003);
const uint32x4_t K32_00000004 = SIMD_VEC_SET1_EPI32(0x00000004);
const uint32x4_t K32_00000005 = SIMD_VEC_SET1_EPI32(0x00000005);
const uint32x4_t K32_00000008 = SIMD_VEC_SET1_EPI32(0x00000008);
const uint32x4_t K32_00000010 = SIMD_VEC_SET1_EPI32(0x00000010);
const uint32x4_t K32_000000FF = SIMD_VEC_SET1_EPI32(0x000000FF);
const uint32x4_t K32_0000FFFF = SIMD_VEC_SET1_EPI32(0x0000FFFF);
const uint32x4_t K32_00010000 = SIMD_VEC_SET1_EPI32(0x00010000);
const uint32x4_t K32_01000000 = SIMD_VEC_SET1_EPI32(0x01000000);
const uint32x4_t K32_08080800 = SIMD_VEC_SET1_EPI32(0x08080800);
const uint32x4_t K32_FFFFFF00 = SIMD_VEC_SET1_EPI32(0xFFFFFF00);
const uint32x4_t K32_FFFFFFFF = SIMD_VEC_SET1_EPI32(0xFFFFFFFF);
const uint32x4_t K32_0123 = SIMD_VEC_SETR_EPI32(0, 1, 2, 3);

const uint64x2_t K64_0000000000000000 = SIMD_VEC_SET1_EPI64(0x0000000000000000);

const uint16x4_t K16_BLUE_TO_GRAY_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_GRAY_WEIGHT);
const uint16x4_t K16_GREEN_TO_GRAY_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_GRAY_WEIGHT);
const uint16x4_t K16_RED_TO_GRAY_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_GRAY_WEIGHT);
const uint32x4_t K32_BGR_TO_GRAY_ROUND_TERM = SIMD_VEC_SET1_EPI32(Base::BGR_TO_GRAY_ROUND_TERM);

const int16x8_t K16_Y_ADJUST = SIMD_VEC_SET1_EPI16(Base::Y_ADJUST);
const int16x8_t K16_UV_ADJUST = SIMD_VEC_SET1_EPI16(Base::UV_ADJUST);

const int16x4_t K16_BLUE_TO_Y_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_Y_WEIGHT);
const int16x4_t K16_GREEN_TO_Y_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_Y_WEIGHT);
const int16x4_t K16_RED_TO_Y_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_Y_WEIGHT);

const int16x4_t K16_BLUE_TO_U_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_U_WEIGHT);
const int16x4_t K16_GREEN_TO_U_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_U_WEIGHT);
const int16x4_t K16_RED_TO_U_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_U_WEIGHT);

const int16x4_t K16_BLUE_TO_V_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_V_WEIGHT);
const int16x4_t K16_GREEN_TO_V_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_V_WEIGHT);
const int16x4_t K16_RED_TO_V_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_V_WEIGHT);

const int32x4_t K32_BGR_TO_YUV_ROUND_TERM = SIMD_VEC_SET1_EPI32(Base::BGR_TO_YUV_ROUND_TERM);

const int16x4_t K16_Y_TO_RGB_WEIGHT = SIMD_VEC_SET1_PI16(Base::Y_TO_RGB_WEIGHT);

const int16x4_t K16_U_TO_BLUE_WEIGHT = SIMD_VEC_SET1_PI16(Base::U_TO_BLUE_WEIGHT);
const int16x4_t K16_U_TO_GREEN_WEIGHT = SIMD_VEC_SET1_PI16(Base::U_TO_GREEN_WEIGHT);

const int16x4_t K16_V_TO_GREEN_WEIGHT = SIMD_VEC_SET1_PI16(Base::V_TO_GREEN_WEIGHT);
const int16x4_t K16_V_TO_RED_WEIGHT = SIMD_VEC_SET1_PI16(Base::V_TO_RED_WEIGHT);

const int32x4_t K32_YUV_TO_BGR_ROUND_TERM = SIMD_VEC_SET1_EPI32(Base::YUV_TO_BGR_ROUND_TERM);
}  // namespace Neon
#endif  // SIMD_NEON_ENABLE

SIMD_INLINE size_t DivHi(size_t value, size_t divider)
{
    return (value + divider - 1) / divider;
}

SIMD_INLINE size_t Pow2Hi(size_t value)
{
    size_t pow2 = 1;
    for (; pow2 < value; pow2 *= 2)
        ;
    return pow2;
}

SIMD_INLINE size_t AlignHiAny(size_t size, size_t align)
{
    return (size + align - 1) / align * align;
}

SIMD_INLINE size_t AlignLoAny(size_t size, size_t align)
{
    return size / align * align;
}

SIMD_INLINE size_t AlignHi(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

SIMD_INLINE void *AlignHi(const void *ptr, size_t align)
{
    return (void *)((((size_t)ptr) + align - 1) & ~(align - 1));
}

SIMD_INLINE size_t AlignLo(size_t size, size_t align)
{
    return size & ~(align - 1);
}

SIMD_INLINE void *AlignLo(const void *ptr, size_t align)
{
    return (void *)(((size_t)ptr) & ~(align - 1));
}

SIMD_INLINE bool Aligned(size_t size, size_t align)
{
    return size == AlignLo(size, align);
}

SIMD_INLINE bool Aligned(const void *ptr, size_t align)
{
    return ptr == AlignLo(ptr, align);
}

SIMD_INLINE void *Allocate(size_t size, size_t align = SIMD_ALIGN)
{
    void *ptr = NULL;
    align = AlignHi(align, sizeof(void *));
    size = AlignHi(size, align);
    int result = ::posix_memalign(&ptr, align, size);
    if (result != 0)
        ptr = NULL;
    return ptr;
}

template <class T>
T *Allocate(uint8_t *&buffer, size_t size, size_t align = SIMD_ALIGN)
{
    T *ptr = (T *)buffer;
    buffer = buffer + AlignHi(size * sizeof(T), align);
    return ptr;
}

SIMD_INLINE void Free(void *ptr)
{
    free(ptr);
}

struct Deletable
{
    virtual ~Deletable() {}
};

#ifdef SIMD_NEON_ENABLE
namespace Neon
{
SIMD_INLINE bool Aligned(size_t size, size_t align = sizeof(uint8x16_t))
{
    return Simd::Aligned(size, align);
}

SIMD_INLINE bool Aligned(const void *ptr, size_t align = sizeof(uint8x16_t))
{
    return Simd::Aligned(ptr, align);
}
}  // namespace Neon

template <bool align>
SIMD_INLINE uint8x16_t Load(const uint8_t *p);

template <>
SIMD_INLINE uint8x16_t Load<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld1q_u8(p);
}

template <>
SIMD_INLINE uint8x16_t Load<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    return vld1q_u8(_p);
#elif defined(_MSC_VER)
    return vld1q_u8_ex(p, 128);
#else
    return vld1q_u8(p);
#endif
}

template <bool align>
SIMD_INLINE int8x16_t Load(const int8_t *p)
{
    return (int8x16_t)Load<align>((const uint8_t *)p);
}

template <bool align>
SIMD_INLINE int16x8_t Load(const int16_t *p)
{
    return (int16x8_t)Load<align>((const uint8_t *)p);
}

template <bool align>
SIMD_INLINE uint16x8_t Load(const uint16_t *p)
{
    return (uint16x8_t)Load<align>((const uint8_t *)p);
}

template <bool align>
SIMD_INLINE int32x4_t Load(const int32_t *p)
{
    return (int32x4_t)Load<align>((const uint8_t *)p);
}

template <bool align>
SIMD_INLINE uint32x4_t Load(const uint32_t *p)
{
    return (uint32x4_t)Load<align>((const uint8_t *)p);
}

template <bool align>
SIMD_INLINE float32x4_t Load(const float *p);

template <>
SIMD_INLINE float32x4_t Load<false>(const float *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld1q_f32(p);
}

template <>
SIMD_INLINE float32x4_t Load<true>(const float *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    return vld1q_f32(_p);
#elif defined(_MSC_VER)
    return vld1q_f32_ex(p, 128);
#else
    return vld1q_f32(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x16x2_t Load2(const uint8_t *p);

template <>
SIMD_INLINE uint8x16x2_t Load2<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld2q_u8(p);
}

template <>
SIMD_INLINE uint8x16x2_t Load2<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    return vld2q_u8(_p);
#elif defined(_MSC_VER)
    return vld2q_u8_ex(p, 128);
#else
    return vld2q_u8(p);
#endif
}

template <bool align>
SIMD_INLINE uint16x8x2_t Load2(const uint16_t *p);

template <>
SIMD_INLINE uint16x8x2_t Load2<false>(const uint16_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld2q_u16(p);
}

template <>
SIMD_INLINE uint16x8x2_t Load2<true>(const uint16_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint16_t *_p = (uint16_t *)__builtin_assume_aligned(p, 16);
    return vld2q_u16(_p);
#elif defined(_MSC_VER)
    return vld2q_u16_ex(p, 128);
#else
    return vld2q_u16(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x16x3_t Load3(const uint8_t *p);

template <>
SIMD_INLINE uint8x16x3_t Load3<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld3q_u8(p);
}

template <>
SIMD_INLINE uint8x16x3_t Load3<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    return vld3q_u8(_p);
#elif defined(_MSC_VER)
    return vld3q_u8_ex(p, 128);
#else
    return vld3q_u8(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x16x4_t Load4(const uint8_t *p);

template <>
SIMD_INLINE uint8x16x4_t Load4<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld4q_u8(p);
}

template <>
SIMD_INLINE uint8x16x4_t Load4<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    return vld4q_u8(_p);
#elif defined(_MSC_VER)
    return vld4q_u8_ex(p, 128);
#else
    return vld4q_u8(p);
#endif
}

template <bool align>
SIMD_INLINE float32x4x2_t Load2(const float *p);

template <>
SIMD_INLINE float32x4x2_t Load2<false>(const float *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld2q_f32(p);
}

template <>
SIMD_INLINE float32x4x2_t Load2<true>(const float *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    return vld2q_f32(_p);
#elif defined(_MSC_VER)
    return vld2q_f32_ex(p, 128);
#else
    return vld2q_f32(p);
#endif
}

template <bool align>
SIMD_INLINE float32x4x3_t Load3(const float *p);

template <>
SIMD_INLINE float32x4x3_t Load3<false>(const float *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld3q_f32(p);
}

template <>
SIMD_INLINE float32x4x3_t Load3<true>(const float *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    return vld3q_f32(_p);
#elif defined(_MSC_VER)
    return vld3q_f32_ex(p, 128);
#else
    return vld3q_f32(p);
#endif
}

template <bool align>
SIMD_INLINE float32x4x4_t Load4(const float *p);

template <>
SIMD_INLINE float32x4x4_t Load4<false>(const float *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld4q_f32(p);
}

template <>
SIMD_INLINE float32x4x4_t Load4<true>(const float *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    return vld4q_f32(_p);
#elif defined(_MSC_VER)
    return vld4q_f32_ex(p, 128);
#else
    return vld4q_f32(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x8_t LoadHalf(const uint8_t *p);

template <>
SIMD_INLINE uint8x8_t LoadHalf<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld1_u8(p);
}

template <>
SIMD_INLINE uint8x8_t LoadHalf<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    return vld1_u8(_p);
#elif defined(_MSC_VER)
    return vld1_u8_ex(p, 64);
#else
    return vld1_u8(p);
#endif
}

template <bool align>
SIMD_INLINE uint16x4_t LoadHalf(const uint16_t *p)
{
    return (uint16x4_t)LoadHalf<align>((const uint8_t *)p);
}

template <bool align>
SIMD_INLINE float32x2_t LoadHalf(const float *p);

template <>
SIMD_INLINE float32x2_t LoadHalf<false>(const float *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld1_f32(p);
}

template <>
SIMD_INLINE float32x2_t LoadHalf<true>(const float *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    float *_p = (float *)__builtin_assume_aligned(p, 8);
    return vld1_f32(_p);
#elif defined(_MSC_VER)
    return vld1_f32_ex(p, 64);
#else
    return vld1_f32(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x8x2_t LoadHalf2(const uint8_t *p);

template <>
SIMD_INLINE uint8x8x2_t LoadHalf2<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld2_u8(p);
}

template <>
SIMD_INLINE uint8x8x2_t LoadHalf2<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    return vld2_u8(_p);
#elif defined(_MSC_VER)
    return vld2_u8_ex(p, 64);
#else
    return vld2_u8(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x8x3_t LoadHalf3(const uint8_t *p);

template <>
SIMD_INLINE uint8x8x3_t LoadHalf3<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld3_u8(p);
}

template <>
SIMD_INLINE uint8x8x3_t LoadHalf3<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    return vld3_u8(_p);
#elif defined(_MSC_VER)
    return vld3_u8_ex(p, 64);
#else
    return vld3_u8(p);
#endif
}

template <bool align>
SIMD_INLINE uint8x8x4_t LoadHalf4(const uint8_t *p);

template <>
SIMD_INLINE uint8x8x4_t LoadHalf4<false>(const uint8_t *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld4_u8(p);
}

template <>
SIMD_INLINE uint8x8x4_t LoadHalf4<true>(const uint8_t *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    return vld4_u8(_p);
#elif defined(_MSC_VER)
    return vld4_u8_ex(p, 64);
#else
    return vld4_u8(p);
#endif
}

template <bool align>
SIMD_INLINE float32x2x4_t LoadHalf4(const float *p);

template <>
SIMD_INLINE float32x2x4_t LoadHalf4<false>(const float *p)
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    return vld4_f32(p);
}

template <>
SIMD_INLINE float32x2x4_t LoadHalf4<true>(const float *p)
{
#if defined(__GNUC__)
#if SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    float *_p = (float *)__builtin_assume_aligned(p, 8);
    return vld4_f32(_p);
#elif defined(_MSC_VER)
    return vld4_f32_ex(p, 64);
#else
    return vld4_f32(p);
#endif
}

template <size_t count>
SIMD_INLINE uint8x16_t LoadBeforeFirst(uint8x16_t first)
{
    return vextq_u8(vextq_u8(first, first, count), first, 16 - count);
}

template <size_t count>
SIMD_INLINE uint8x16_t LoadAfterLast(uint8x16_t last)
{
    return vextq_u8(last, vextq_u8(last, last, 16 - count), count);
}

template <bool align, size_t step>
SIMD_INLINE void LoadNose3(const uint8_t *p, uint8x16_t a[3])
{
    a[1] = Load<align>(p);
    a[0] = LoadBeforeFirst<step>(a[1]);
    a[2] = vld1q_u8(p + step);
}

template <bool align, size_t step>
SIMD_INLINE void LoadBody3(const uint8_t *p, uint8x16_t a[3])
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    a[0] = vld1q_u8(p - step);
    a[1] = Load<align>(p);
    a[2] = vld1q_u8(p + step);
}

template <bool align, size_t step>
SIMD_INLINE void LoadTail3(const uint8_t *p, uint8x16_t a[3])
{
    a[0] = vld1q_u8(p - step);
    a[1] = Load<align>(p);
    a[2] = LoadAfterLast<step>(a[1]);
}

template <bool align, size_t step>
SIMD_INLINE void LoadNose5(const uint8_t *p, uint8x16_t a[5])
{
    a[2] = Load<align>(p);
    a[1] = LoadBeforeFirst<step>(a[2]);
    a[0] = LoadBeforeFirst<step>(a[1]);
    a[3] = vld1q_u8(p + step);
    a[4] = vld1q_u8(p + 2 * step);
}

template <bool align, size_t step>
SIMD_INLINE void LoadBody5(const uint8_t *p, uint8x16_t a[5])
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    a[0] = vld1q_u8(p - 2 * step);
    a[1] = vld1q_u8(p - step);
    a[2] = Load<align>(p);
    a[3] = vld1q_u8(p + step);
    a[4] = vld1q_u8(p + 2 * step);
}

template <bool align, size_t step>
SIMD_INLINE void LoadTail5(const uint8_t *p, uint8x16_t a[5])
{
    a[0] = vld1q_u8(p - 2 * step);
    a[1] = vld1q_u8(p - step);
    a[2] = Load<align>(p);
    a[3] = LoadAfterLast<step>(a[2]);
    a[4] = LoadAfterLast<step>(a[3]);
}

SIMD_INLINE void LoadNoseDx(const uint8_t *p, uint8x16_t a[3])
{
    a[0] = LoadBeforeFirst<1>(vld1q_u8(p));
    a[2] = vld1q_u8(p + 1);
}

SIMD_INLINE void LoadBodyDx(const uint8_t *p, uint8x16_t a[3])
{
#if defined(__GNUC__) && SIMD_NEON_PREFECH_SIZE
    __builtin_prefetch(p + SIMD_NEON_PREFECH_SIZE);
#endif
    a[0] = vld1q_u8(p - 1);
    a[2] = vld1q_u8(p + 1);
}

SIMD_INLINE void LoadTailDx(const uint8_t *p, uint8x16_t a[3])
{
    a[0] = vld1q_u8(p - 1);
    a[2] = LoadAfterLast<1>(vld1q_u8(p));
}

template <size_t count>
SIMD_INLINE uint8x8_t LoadBeforeFirst(uint8x8_t first)
{
    return vext_u8(vext_u8(first, first, count), first, 8 - count);
}

template <size_t count>
SIMD_INLINE uint8x8_t LoadAfterLast(uint8x8_t last)
{
    return vext_u8(last, vext_u8(last, last, 8 - count), count);
}

SIMD_INLINE float32x4_t Load(const float *p0, const float *p1)
{
    return vcombine_f32(vld1_f32(p0), vld1_f32(p1));
}

SIMD_INLINE float32x4_t LoadPadZeroNose1(const float *p)
{
    return vextq_f32(vdupq_n_f32(0.0f), Load<false>(p + 1), 3);
}

SIMD_INLINE float32x4_t LoadPadZeroTail1(const float *p)
{
    return vextq_f32(Load<false>(p - 1), vdupq_n_f32(0.0f), 1);
}

SIMD_INLINE float32x4_t LoadPadZeroTail2(const float *p)
{
    return vextq_f32(Load<false>(p - 2), vdupq_n_f32(0.0f), 2);
}

template <bool align>
SIMD_INLINE void Store(uint8_t *p, uint8x16_t a);

template <>
SIMD_INLINE void Store<false>(uint8_t *p, uint8x16_t a)
{
    vst1q_u8(p, a);
}

template <>
SIMD_INLINE void Store<true>(uint8_t *p, uint8x16_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    vst1q_u8(_p, a);
#elif defined(_MSC_VER)
    vst1q_u8_ex(p, a, 128);
#else
    vst1q_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store(uint8_t *p, uint8x8_t a);

template <>
SIMD_INLINE void Store<false>(uint8_t *p, uint8x8_t a)
{
    vst1_u8(p, a);
}

template <>
SIMD_INLINE void Store<true>(uint8_t *p, uint8x8_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    vst1_u8(_p, a);
#elif defined(_MSC_VER)
    vst1_u8_ex(p, a, 64);
#else
    vst1_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store(uint16_t *p, uint16x8_t a)
{
    Store<align>((uint8_t *)p, (uint8x16_t)a);
}

template <bool align>
SIMD_INLINE void Store(uint16_t *p, uint16x4_t a)
{
    Store<align>((uint8_t *)p, (uint8x8_t)a);
}

template <bool align>
SIMD_INLINE void Store(int16_t *p, int16x8_t a)
{
    Store<align>((uint8_t *)p, (uint8x16_t)a);
}

template <bool align>
SIMD_INLINE void Store(uint32_t *p, uint32x4_t a)
{
    Store<align>((uint8_t *)p, (uint8x16_t)a);
}

template <bool align>
SIMD_INLINE void Store(int32_t *p, int32x4_t a)
{
    Store<align>((uint8_t *)p, (uint8x16_t)a);
}

template <bool align>
SIMD_INLINE void Store2(uint8_t *p, uint8x16x2_t a);

template <>
SIMD_INLINE void Store2<false>(uint8_t *p, uint8x16x2_t a)
{
    vst2q_u8(p, a);
}

template <>
SIMD_INLINE void Store2<true>(uint8_t *p, uint8x16x2_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    vst2q_u8(_p, a);
#elif defined(_MSC_VER)
    vst2q_u8_ex(p, a, 128);
#else
    vst2q_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store2(int16_t *p, int16x8x2_t a);

template <>
SIMD_INLINE void Store2<false>(int16_t *p, int16x8x2_t a)
{
    vst2q_s16(p, a);
}

template <>
SIMD_INLINE void Store2<true>(int16_t *p, int16x8x2_t a)
{
#if defined(__GNUC__)
    int16_t *_p = (int16_t *)__builtin_assume_aligned(p, 16);
    vst2q_s16(_p, a);
#elif defined(_MSC_VER)
    vst2q_s16_ex(p, a, 128);
#else
    vst2q_s16(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store2(uint8_t *p, uint8x8x2_t a);

template <>
SIMD_INLINE void Store2<false>(uint8_t *p, uint8x8x2_t a)
{
    vst2_u8(p, a);
}

template <>
SIMD_INLINE void Store2<true>(uint8_t *p, uint8x8x2_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    vst2_u8(_p, a);
#elif defined(_MSC_VER)
    vst2_u8_ex(p, a, 64);
#else
    vst2_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store3(uint8_t *p, uint8x16x3_t a);

template <>
SIMD_INLINE void Store3<false>(uint8_t *p, uint8x16x3_t a)
{
    vst3q_u8(p, a);
}

template <>
SIMD_INLINE void Store3<true>(uint8_t *p, uint8x16x3_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    vst3q_u8(_p, a);
#elif defined(_MSC_VER)
    vst3q_u8_ex(p, a, 128);
#else
    vst3q_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store3(uint8_t *p, uint8x8x3_t a);

template <>
SIMD_INLINE void Store3<false>(uint8_t *p, uint8x8x3_t a)
{
    vst3_u8(p, a);
}

template <>
SIMD_INLINE void Store3<true>(uint8_t *p, uint8x8x3_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    vst3_u8(_p, a);
#elif defined(_MSC_VER)
    vst3_u8_ex(p, a, 64);
#else
    vst3_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store4(uint8_t *p, uint8x16x4_t a);

template <>
SIMD_INLINE void Store4<false>(uint8_t *p, uint8x16x4_t a)
{
    vst4q_u8(p, a);
}

template <>
SIMD_INLINE void Store4<true>(uint8_t *p, uint8x16x4_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 16);
    vst4q_u8(_p, a);
#elif defined(_MSC_VER)
    vst4q_u8_ex(p, a, 128);
#else
    vst4q_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store4(uint8_t *p, uint8x8x4_t a);

template <>
SIMD_INLINE void Store4<false>(uint8_t *p, uint8x8x4_t a)
{
    vst4_u8(p, a);
}

template <>
SIMD_INLINE void Store4<true>(uint8_t *p, uint8x8x4_t a)
{
#if defined(__GNUC__)
    uint8_t *_p = (uint8_t *)__builtin_assume_aligned(p, 8);
    vst4_u8(_p, a);
#elif defined(_MSC_VER)
    vst4_u8_ex(p, a, 64);
#else
    vst4_u8(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store(float *p, float32x4_t a);

template <>
SIMD_INLINE void Store<false>(float *p, float32x4_t a)
{
    vst1q_f32(p, a);
}

template <>
SIMD_INLINE void Store<true>(float *p, float32x4_t a)
{
#if defined(__GNUC__)
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    vst1q_f32(_p, a);
#elif defined(_MSC_VER)
    vst1q_f32_ex(p, a, 128);
#else
    vst1q_f32(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store(float *p, float32x2_t a);

template <>
SIMD_INLINE void Store<false>(float *p, float32x2_t a)
{
    vst1_f32(p, a);
}

template <>
SIMD_INLINE void Store<true>(float *p, float32x2_t a)
{
#if defined(__GNUC__)
    float *_p = (float *)__builtin_assume_aligned(p, 8);
    vst1_f32(_p, a);
#elif defined(_MSC_VER)
    vst1_f32_ex(p, a, 64);
#else
    vst1_f32(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store2(float *p, float32x4x2_t a);

template <>
SIMD_INLINE void Store2<false>(float *p, float32x4x2_t a)
{
    vst2q_f32(p, a);
}

template <>
SIMD_INLINE void Store2<true>(float *p, float32x4x2_t a)
{
#if defined(__GNUC__)
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    vst2q_f32(_p, a);
#elif defined(_MSC_VER)
    vst2q_f32_ex(p, a, 128);
#else
    vst2q_f32(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store3(float *p, float32x4x3_t a);

template <>
SIMD_INLINE void Store3<false>(float *p, float32x4x3_t a)
{
    vst3q_f32(p, a);
}

template <>
SIMD_INLINE void Store3<true>(float *p, float32x4x3_t a)
{
#if defined(__GNUC__)
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    vst3q_f32(_p, a);
#elif defined(_MSC_VER)
    vst3q_f32_ex(p, a, 128);
#else
    vst3q_f32(p, a);
#endif
}

template <bool align>
SIMD_INLINE void Store4(float *p, float32x4x4_t a);

template <>
SIMD_INLINE void Store4<false>(float *p, float32x4x4_t a)
{
    vst4q_f32(p, a);
}

template <>
SIMD_INLINE void Store4<true>(float *p, float32x4x4_t a)
{
#if defined(__GNUC__)
    float *_p = (float *)__builtin_assume_aligned(p, 16);
    vst4q_f32(_p, a);
#elif defined(_MSC_VER)
    vst4q_f32_ex(p, a, 128);
#else
    vst4q_f32(p, a);
#endif
}

template <bool align>
SIMD_INLINE void StoreMasked(float *p, float32x4_t value, uint32x4_t mask)
{
    float32x4_t old = Load<align>(p);
    Store<align>(p, vbslq_f32(mask, value, old));
}

SIMD_INLINE uint8x16_t PackU16(uint16x8_t lo, uint16x8_t hi)
{
    return vcombine_u8(vmovn_u16(lo), vmovn_u16(hi));
}

namespace Neon
{
void ResizeBilinearGray(const uint8_t *src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t *dst,
                        size_t dstWidth, size_t dstHeight, size_t dstStride);

void ResizeBilinear(const uint8_t *src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t *dst,
                    size_t dstWidth, size_t dstHeight, size_t dstStride, size_t channelCount);
}  // namespace Neon
#endif
}  // namespace Simd

namespace Simd
{
namespace Base
{
void ResizeBilinear(const uint8_t *src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t *dst,
                    size_t dstWidth, size_t dstHeight, size_t dstStride, size_t channelCount);
}
}  // namespace Simd
#endif  //__SimdBase_h__
