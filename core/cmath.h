/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL: the small maths, in single precision, without a C library (the FM-1 has none; and the same results on
 * the PC, so the prototype sounds as the unit will). Accuracy: what a plant model and a sound need, no more. */
#ifndef CMATH_H
#define CMATH_H
#include <stdint.h>

#define C_PI 3.14159265f
#define C_TWO_PI 6.28318531f
#define C_INLINE static inline __attribute__((always_inline))

C_INLINE float c_absf(float x) { return x < 0 ? -x : x; }
C_INLINE float c_clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

/* 2^x, |x| < 31: Taylor on the fraction (rel. error < 2e-7) */
C_INLINE float c_exp2(float x)
{
    if (x < -30.0f) return 0.0f;
    if (x > 30.0f) x = 30.0f;
    int i = (int)x; if ((float)i > x) i--;
    float f = x - (float)i;
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * (0.00961813f + f * (0.00133336f +
              f * (0.00015404f + f * (0.00001525f + f * 0.00000132f)))))));
    union { float f; uint32_t u; } v; v.f = p; v.u += (uint32_t)i << 23;
    return v.f;
}
C_INLINE float c_exp(float x) { return c_exp2(x * 1.44269504f); }
/* log2 x, x > 0: the exponent, then ln of the mantissa in [√½, √2) by 2 atanh((m-1)/(m+1)) (abs. error < 1e-7) */
C_INLINE float c_log2(float x)
{
    if (x <= 1e-30f) return -100.0f;
    union { float f; uint32_t u; } v; v.f = x;
    int e = (int)((v.u >> 23) & 255) - 127;
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;               /* m in [1, 2) */
    float m = v.f;
    if (m > 1.41421356f) { m *= 0.5f; e++; }
    float s = (m - 1.0f) / (m + 1.0f), s2 = s * s;
    float ln = 2.0f * s * (1.0f + s2 * (0.33333333f + s2 * (0.2f + s2 * 0.14285714f)));
    return (float)e + ln * 1.44269504f;
}
C_INLINE float c_log10(float x) { return c_log2(x) * 0.30103f; }
C_INLINE float c_ln(float x) { return c_log2(x) * 0.69314718f; }
C_INLINE float c_pow(float x, float y) { return x <= 0 ? 0.0f : c_exp2(y * c_log2(x)); }
/* sin x for any x: reduced to [-π, π], then an odd polynomial (abs. error < 2e-6) */
C_INLINE float c_sin(float x)
{
    float k = x * (1.0f / C_TWO_PI);
    int i = (int)(k + (k >= 0 ? 0.5f : -0.5f));
    x -= (float)i * C_TWO_PI;
    if (x > C_PI * 0.5f) x = C_PI - x; else if (x < -C_PI * 0.5f) x = -C_PI - x;
    float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333310f + x2 * (-0.00019840874f + x2 * 2.7525562e-6f))));
}
C_INLINE float c_cos(float x) { return c_sin(x + C_PI * 0.5f); }
/* tanh: a rational fit, exact at 0, saturating at ±1 (error < 0.02: a soft clip's need) */
C_INLINE float c_tanh(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
C_INLINE float c_sqrt(float x)
{
    if (x <= 0) return 0.0f;
    union { float f; uint32_t u; } v; v.f = x; v.u = (v.u >> 1) + 0x1FC00000u;   /* (a first guess, then Newton) */
    float y = v.f;
    y = 0.5f * (y + x / y); y = 0.5f * (y + x / y); y = 0.5f * (y + x / y);
    return y;
}
#endif
