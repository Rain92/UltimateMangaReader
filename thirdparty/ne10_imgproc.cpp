#include "ne10_imgproc.h"

// code taken from ne10 project

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define INTER_RESIZE_COEF_BITS 11
#define INTER_RESIZE_COEF_SCALE (1 << 11)
#define BITS (INTER_RESIZE_COEF_BITS * 2)
#define DELTA (1 << (INTER_RESIZE_COEF_BITS * 2 - 1))
#define MAX_ESIZE 16

#define VRESIZE_LINEAR_MASK_TABLE_SIZE 7

const uint64_t img_vresize_linear_mask_residual_table[VRESIZE_LINEAR_MASK_TABLE_SIZE] = {
    0x00000000000000FF, 0x000000000000FFFF, 0x0000000000FFFFFF, 0x00000000FFFFFFFF,
    0x000000FFFFFFFFFF, 0x0000FFFFFFFFFFFF, 0x00FFFFFFFFFFFFFF};

inline uint32_t align_size(int32_t sz, int32_t n)
{
    return (sz + n - 1) & -n;
}

inline int32_t floor(float a)
{
    return (((a) >= 0) ? ((int32_t)a) : ((int32_t)a - 1));
}

inline int32_t clip(int32_t x, int32_t a, int32_t b)
{
    return (x >= a ? (x < b ? x : b - 1) : a);
}

inline uint8_t cast_op(int32_t val)
{
    int32_t temp = MIN(255, MAX(0, (val + DELTA) >> BITS));
    return (uint8_t)(temp);
};

void img_hresize_linear_c(const uint8_t** src, int32_t** dst, int32_t count, const int32_t* xofs,
                          const int16_t* alpha, int32_t swidth, int32_t dwidth, int32_t cn, int32_t xmin,
                          int32_t xmax)
{
    int32_t dx, k;

    int32_t dx0 = 0;

    // for (k = 0; k <= count - 2; k++)
    if (count == 2)
    {
        k = 0;
        const uint8_t *S0 = src[k], *S1 = src[k + 1];
        int32_t *D0 = dst[k], *D1 = dst[k + 1];
        for (dx = dx0; dx < xmax; dx++)
        {
            int32_t sx = xofs[dx];
            int32_t a0 = alpha[dx * 2], a1 = alpha[dx * 2 + 1];
            int32_t t0 = S0[sx] * a0 + S0[sx + cn] * a1;
            int32_t t1 = S1[sx] * a0 + S1[sx + cn] * a1;
            D0[dx] = t0;
            D1[dx] = t1;
        }

        for (; dx < dwidth; dx++)
        {
            int32_t sx = xofs[dx];
            D0[dx] = (int32_t)S0[sx] * INTER_RESIZE_COEF_SCALE;
            D1[dx] = (int32_t)S1[sx] * INTER_RESIZE_COEF_SCALE;
        }
    }

    // for (; k < count; k++)
    if (count == 1)
    {
        k = 0;
        const uint8_t* S = src[k];
        int32_t* D = dst[k];
        for (dx = 0; dx < xmax; dx++)
        {
            int32_t sx = xofs[dx];
            D[dx] = S[sx] * alpha[dx * 2] + S[sx + cn] * alpha[dx * 2 + 1];
        }

        for (; dx < dwidth; dx++)
            D[dx] = (int32_t)S[xofs[dx]] * INTER_RESIZE_COEF_SCALE;
    }
}

void img_vresize_linear_c(const int32_t** src, uint8_t* dst, const int16_t* beta, int32_t width)
{
    int32_t b0 = beta[0], b1 = beta[1];
    const int32_t *S0 = src[0], *S1 = src[1];

    int32_t x = 0;
    for (; x <= width - 4; x += 4)
    {
        int32_t t0, t1;
        t0 = S0[x] * b0 + S1[x] * b1;
        t1 = S0[x + 1] * b0 + S1[x + 1] * b1;
        dst[x] = cast_op(t0);
        dst[x + 1] = cast_op(t1);
        t0 = S0[x + 2] * b0 + S1[x + 2] * b1;
        t1 = S0[x + 3] * b0 + S1[x + 3] * b1;
        dst[x + 2] = cast_op(t0);
        dst[x + 3] = cast_op(t1);
    }

    for (; x < width; x++)
        dst[x] = cast_op(S0[x] * b0 + S1[x] * b1);
}

void img_resize_generic_linear_c(uint8_t* src, uint8_t* dst, const int32_t* xofs, const int16_t* _alpha,
                                 const int32_t* yofs, const int16_t* _beta, int32_t xmin, int32_t xmax,
                                 int32_t ksize, int32_t srcw, int32_t srch, int32_t srcstep, int32_t dstw,
                                 int32_t dsth, int32_t channels)
{
    const int16_t* alpha = _alpha;
    const int16_t* beta = _beta;
    int32_t cn = channels;
    srcw *= cn;
    dstw *= cn;

    int32_t bufstep = (int32_t)align_size(dstw, 16);
    int32_t dststep = (int32_t)align_size(dstw, 4);

    int32_t* buffer_ = (int32_t*)malloc(bufstep * ksize * sizeof(int32_t));

    const uint8_t* srows[MAX_ESIZE];
    int32_t* rows[MAX_ESIZE];
    int32_t prev_sy[MAX_ESIZE];
    int32_t k, dy;
    xmin *= cn;
    xmax *= cn;

    for (k = 0; k < ksize; k++)
    {
        prev_sy[k] = -1;
        rows[k] = (int32_t*)buffer_ + bufstep * k;
    }

    // image resize is a separable operation. In case of not too strong
    for (dy = 0; dy < dsth; dy++, beta += ksize)
    {
        int32_t sy0 = yofs[dy], k, k0 = ksize, k1 = 0, ksize2 = ksize / 2;

        for (k = 0; k < ksize; k++)
        {
            int32_t sy = clip(sy0 - ksize2 + 1 + k, 0, srch);
            for (k1 = MAX(k1, k); k1 < ksize; k1++)
            {
                if (sy == prev_sy[k1])  // if the sy-th row has been computed already, reuse it.
                {
                    if (k1 > k)
                        memcpy(rows[k], rows[k1], bufstep * sizeof(rows[0][0]));
                    break;
                }
            }
            if (k1 == ksize)
                k0 = MIN(k0, k);  // remember the first row that needs to be computed
            srows[k] = (const uint8_t*)(src + srcstep * sy);
            prev_sy[k] = sy;
        }

        if (k0 < ksize)
            img_hresize_linear_c(srows + k0, rows + k0, ksize - k0, xofs, alpha, srcw, dstw, cn, xmin, xmax);

        img_vresize_linear_c((const int32_t**)rows, (uint8_t*)(dst + dststep * dy), beta, dstw);
    }

    free(buffer_);
}

void img_resize_cal_offset_linear(int32_t* xofs, int16_t* ialpha, int32_t* yofs, int16_t* ibeta,
                                  int32_t* xmin, int32_t* xmax, int32_t ksize, int32_t ksize2, int32_t srcw,
                                  int32_t srch, int32_t dstw, int32_t dsth, int32_t channels)
{
    float inv_scale_x = (float)dstw / srcw;
    float inv_scale_y = (float)dsth / srch;

    int32_t cn = channels;
    float scale_x = 1. / inv_scale_x;
    float scale_y = 1. / inv_scale_y;
    int32_t k, sx, sy, dx, dy;

    float fx, fy;

    float cbuf[MAX_ESIZE];

    for (dx = 0; dx < dstw; dx++)
    {
        fx = (float)((dx + 0.5) * scale_x - 0.5);
        sx = floor(fx);
        fx -= sx;

        if (sx < ksize2 - 1)
        {
            *xmin = dx + 1;
            if (sx < 0)
                fx = 0, sx = 0;
        }

        if (sx + ksize2 >= srcw)
        {
            *xmax = MIN(*xmax, dx);
            if (sx >= srcw - 1)
                fx = 0, sx = srcw - 1;
        }

        for (k = 0, sx *= cn; k < cn; k++)
            xofs[dx * cn + k] = sx + k;

        cbuf[0] = 1.f - fx;
        cbuf[1] = fx;

        for (k = 0; k < ksize; k++)
            ialpha[dx * cn * ksize + k] = (int16_t)(cbuf[k] * INTER_RESIZE_COEF_SCALE);
        for (; k < cn * ksize; k++)
            ialpha[dx * cn * ksize + k] = ialpha[dx * cn * ksize + k - ksize];
    }

    for (dy = 0; dy < dsth; dy++)
    {
        fy = (float)((dy + 0.5) * scale_y - 0.5);
        sy = floor(fy);
        fy -= sy;

        yofs[dy] = sy;

        cbuf[0] = 1.f - fy;
        cbuf[1] = fy;

        for (k = 0; k < ksize; k++)
            ibeta[dy * ksize + k] = (int16_t)(cbuf[k] * INTER_RESIZE_COEF_SCALE);
    }
}

void img_resize_bilinear_grey_c(uint8_t* dst, uint32_t dst_width, uint32_t dst_height, uint8_t* src,
                                uint32_t src_width, uint32_t src_height, uint32_t src_stride)
{
    int32_t dstw = dst_width;
    int32_t dsth = dst_height;
    int32_t srcw = src_width;
    int32_t srch = src_height;

    int32_t cn = 1;

    int32_t xmin = 200;
    int32_t xmax = dstw - 200;
    int32_t width = dstw * cn;

    int32_t ksize = 0, ksize2;
    ksize = 2;
    ksize2 = ksize / 2;

    uint8_t* buffer_ = (uint8_t*)malloc((width + dsth) * (sizeof(int32_t) + sizeof(float) * ksize));

    int32_t* xofs = (int32_t*)buffer_;
    int32_t* yofs = xofs + width;
    int16_t* ialpha = (int16_t*)(yofs + dsth);
    int16_t* ibeta = ialpha + width * ksize;

    img_resize_cal_offset_linear(xofs, ialpha, yofs, ibeta, &xmin, &xmax, ksize, ksize2, srcw, srch, dstw,
                                 dsth, cn);

    img_resize_generic_linear_c(src, dst, xofs, ialpha, yofs, ibeta, xmin, xmax, ksize, srcw, srch,
                                src_stride, dstw, dsth, cn);
    free(buffer_);
}

#ifdef KOBO

void img_vresize_linear_neon(const int** src, unsigned char* dst, const short* beta, int width)
{
    const int *S0 = src[0], *S1 = src[1];

    int32x4_t qS0_0123, qS0_4567, qS1_0123, qS1_4567;
    int32x4_t qT_0123, qT_4567;
    int16x4_t dT_0123, dT_4567;
    uint16x8_t qT_01234567;
    uint8x8_t dT_01234567, dDst_01234567;

    int32x2_t dBeta = {};
    dBeta = vset_lane_s32((int)(beta[0]), dBeta, 0);
    dBeta = vset_lane_s32((int)(beta[1]), dBeta, 1);

    int32x4_t qDelta, qMin, qMax;
    qDelta = vdupq_n_s32(DELTA);
    qMin = vdupq_n_s32(0);
    qMax = vdupq_n_s32(255);

    int x = 0;
    for (; x <= width - 8; x += 8)
    {
        qS0_0123 = vld1q_s32(&S0[x]);
        qS0_4567 = vld1q_s32(&S0[x + 4]);
        qS1_0123 = vld1q_s32(&S1[x]);
        qS1_4567 = vld1q_s32(&S1[x + 4]);

        qT_0123 = vmulq_lane_s32(qS0_0123, dBeta, 0);
        qT_4567 = vmulq_lane_s32(qS0_4567, dBeta, 0);
        qT_0123 = vmlaq_lane_s32(qT_0123, qS1_0123, dBeta, 1);
        qT_4567 = vmlaq_lane_s32(qT_4567, qS1_4567, dBeta, 1);

        qT_0123 = vaddq_s32(qT_0123, qDelta);
        qT_4567 = vaddq_s32(qT_4567, qDelta);

        qT_0123 = vshrq_n_s32(qT_0123, BITS);
        qT_4567 = vshrq_n_s32(qT_4567, BITS);

        qT_0123 = vmaxq_s32(qT_0123, qMin);
        qT_4567 = vmaxq_s32(qT_4567, qMin);
        qT_0123 = vminq_s32(qT_0123, qMax);
        qT_4567 = vminq_s32(qT_4567, qMax);

        dT_0123 = vmovn_s32(qT_0123);
        dT_4567 = vmovn_s32(qT_4567);
        qT_01234567 = vreinterpretq_u16_s16(vcombine_s16(dT_0123, dT_4567));
        dT_01234567 = vmovn_u16(qT_01234567);

        vst1_u8(&dst[x], dT_01234567);
    }

    if (x < width)
    {
        uint8x8_t dMask;
        dMask = vld1_u8((uint8_t*)(&img_vresize_linear_mask_residual_table[(width - x - 1)]));
        dDst_01234567 = vld1_u8(&dst[x]);

        qS0_0123 = vld1q_s32(&S0[x]);
        qS0_4567 = vld1q_s32(&S0[x + 4]);
        qS1_0123 = vld1q_s32(&S1[x]);
        qS1_4567 = vld1q_s32(&S1[x + 4]);

        qT_0123 = vmulq_lane_s32(qS0_0123, dBeta, 0);
        qT_4567 = vmulq_lane_s32(qS0_4567, dBeta, 0);
        qT_0123 = vmlaq_lane_s32(qT_0123, qS1_0123, dBeta, 1);
        qT_4567 = vmlaq_lane_s32(qT_4567, qS1_4567, dBeta, 1);

        qT_0123 = vaddq_s32(qT_0123, qDelta);
        qT_4567 = vaddq_s32(qT_4567, qDelta);

        qT_0123 = vshrq_n_s32(qT_0123, BITS);
        qT_4567 = vshrq_n_s32(qT_4567, BITS);

        qT_0123 = vmaxq_s32(qT_0123, qMin);
        qT_4567 = vmaxq_s32(qT_4567, qMin);
        qT_0123 = vminq_s32(qT_0123, qMax);
        qT_4567 = vminq_s32(qT_4567, qMax);

        dT_0123 = vmovn_s32(qT_0123);
        dT_4567 = vmovn_s32(qT_4567);
        qT_01234567 = vreinterpretq_u16_s16(vcombine_s16(dT_0123, dT_4567));
        dT_01234567 = vmovn_u16(qT_01234567);

        dMask = vbsl_u8(dMask, dT_01234567, dDst_01234567);
        vst1_u8(&dst[x], dMask);
    }
}

void img_resize_generic_linear_neon(uint8_t* src, uint8_t* dst, const int32_t* xofs, const int16_t* _alpha,
                                    const int32_t* yofs, const int16_t* _beta, int32_t xmin, int32_t xmax,
                                    int32_t ksize, int32_t srcw, int32_t srch, int32_t srcstep, int32_t dstw,
                                    int32_t dsth, int32_t channels)
{
    const int16_t* alpha = _alpha;
    const int16_t* beta = _beta;
    int32_t cn = channels;
    srcw *= cn;
    dstw *= cn;

    int32_t bufstep = (int32_t)align_size(dstw, 16);
    int32_t dststep = (int32_t)align_size(dstw, 4);

    int32_t* buffer_ = (int32_t*)malloc(bufstep * ksize * sizeof(int32_t));

    const uint8_t* srows[MAX_ESIZE];
    int32_t* rows[MAX_ESIZE];
    int32_t prev_sy[MAX_ESIZE];
    int32_t k, dy;
    xmin *= cn;
    xmax *= cn;

    for (k = 0; k < ksize; k++)
    {
        prev_sy[k] = -1;
        rows[k] = (int32_t*)buffer_ + bufstep * k;
    }

    // image resize is a separable operation. In case of not too strong
    for (dy = 0; dy < dsth; dy++, beta += ksize)
    {
        int32_t sy0 = yofs[dy], k, k0 = ksize, k1 = 0, ksize2 = ksize / 2;

        for (k = 0; k < ksize; k++)
        {
            int32_t sy = clip(sy0 - ksize2 + 1 + k, 0, srch);
            for (k1 = MAX(k1, k); k1 < ksize; k1++)
            {
                if (sy == prev_sy[k1])  // if the sy-th row has been computed already, reuse it.
                {
                    if (k1 > k)
                        memcpy(rows[k], rows[k1], bufstep * sizeof(rows[0][0]));
                    break;
                }
            }
            if (k1 == ksize)
                k0 = MIN(k0, k);  // remember the first row that needs to be computed
            srows[k] = (const uint8_t*)(src + srcstep * sy);
            prev_sy[k] = sy;
        }

        if (k0 < ksize)
        {
            //            if (cn == 4)
            //                img_hresize_4channels_linear_neon(srows + k0, rows + k0, ksize - k0, xofs,
            //                alpha, srcw, dstw,
            //                                                  cn, xmin, xmax);
            //            else
            img_hresize_linear_c(srows + k0, rows + k0, ksize - k0, xofs, alpha, srcw, dstw, cn, xmin, xmax);
        }
        //        img_vresize_linear_c((const int32_t**)rows, (uint8_t*)(dst + dststep * dy), beta, dstw);
        img_vresize_linear_neon((const int32_t**)rows, (uint8_t*)(dst + dststep * dy), beta, dstw);
    }

    free(buffer_);
}

void img_resize_bilinear_grey_neon(uint8_t* dst, uint32_t dst_width, uint32_t dst_height, uint8_t* src,
                                   uint32_t src_width, uint32_t src_height, uint32_t src_stride)
{
    int32_t dstw = dst_width;
    int32_t dsth = dst_height;
    int32_t srcw = src_width;
    int32_t srch = src_height;

    int32_t cn = 1;

    int32_t xmin = 0;
    int32_t xmax = dstw;
    int32_t width = dstw * cn;

    int32_t ksize = 0, ksize2;
    ksize = 2;
    ksize2 = ksize / 2;

    uint8_t* buffer_ = (uint8_t*)malloc((width + dsth) * (sizeof(int32_t) + sizeof(float) * ksize));

    int32_t* xofs = (int32_t*)buffer_;
    int32_t* yofs = xofs + width;
    int16_t* ialpha = (int16_t*)(yofs + dsth);
    int16_t* ibeta = ialpha + width * ksize;

    img_resize_cal_offset_linear(xofs, ialpha, yofs, ibeta, &xmin, &xmax, ksize, ksize2, srcw, srch, dstw,
                                 dsth, cn);

    img_resize_generic_linear_neon(src, dst, xofs, ialpha, yofs, ibeta, xmin, xmax, ksize, srcw, srch,
                                   src_stride, dstw, dsth, cn);
    free(buffer_);
}

#endif
