#ifndef NE10_IMGPROC_H
#define NE10_IMGPROC_H

// code taken from ne10 project

#include <string.h>

#include <cstdint>
#include <cstdlib>

#ifdef KOBO
#include <arm_neon.h>
#endif

inline uint32_t align_size(int32_t sz, int32_t n);
inline int32_t floor(float a);

inline int32_t clip(int32_t x, int32_t a, int32_t b);

inline uint8_t cast_op(int32_t val);

void img_hresize_linear_c(const uint8_t** src, int32_t** dst, int32_t count, const int32_t* xofs,
                          const int16_t* alpha, int32_t swidth, int32_t dwidth, int32_t cn, int32_t xmin,
                          int32_t xmax);
void img_vresize_linear_c(const int32_t** src, uint8_t* dst, const int16_t* beta, int32_t width);
void img_resize_generic_linear_c(uint8_t* src, uint8_t* dst, const int32_t* xofs, const int16_t* _alpha,
                                 const int32_t* yofs, const int16_t* _beta, int32_t xmin, int32_t xmax,
                                 int32_t ksize, int32_t srcw, int32_t srch, int32_t srcstep, int32_t dstw,
                                 int32_t dsth, int32_t channels);

void img_resize_cal_offset_linear(int32_t* xofs, int16_t* ialpha, int32_t* yofs, int16_t* ibeta,
                                  int32_t* xmin, int32_t* xmax, int32_t ksize, int32_t ksize2, int32_t srcw,
                                  int32_t srch, int32_t dstw, int32_t dsth, int32_t channels);

void img_resize_bilinear_grey_c(uint8_t* dst, uint32_t dst_width, uint32_t dst_height, uint8_t* src,
                                uint32_t src_width, uint32_t src_height, uint32_t src_stride);
#ifdef KOBO

void img_vresize_linear_neon(const int** src, unsigned char* dst, const short* beta, int width);

void img_resize_generic_linear_neon(uint8_t* src, uint8_t* dst, const int32_t* xofs, const int16_t* _alpha,
                                    const int32_t* yofs, const int16_t* _beta, int32_t xmin, int32_t xmax,
                                    int32_t ksize, int32_t srcw, int32_t srch, int32_t srcstep, int32_t dstw,
                                    int32_t dsth, int32_t channels);

void img_resize_bilinear_grey_neon(uint8_t* dst, uint32_t dst_width, uint32_t dst_height, uint8_t* src,
                                   uint32_t src_width, uint32_t src_height, uint32_t src_stride);

#endif

#endif  // NE10_IMGPROC_H
