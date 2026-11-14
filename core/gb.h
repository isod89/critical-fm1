/* SPDX-License-Identifier: GPL-3.0-only
 * The common base: constants, types, small maths. Portable C11: the same source builds for the PC (tests, the
 * emulator) and for the FM-1 (pi32v2). No register, no OS call here. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __PI32V2__
/* the FM-1 has no C library: core/libc.c gives these, written for this project */
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
#else
#include <string.h>
#endif

#ifndef GB_BLOCK
#define GB_BLOCK 64                 /* frames per audio block: 1.45 ms at 44.1 kHz (phase 0 sets the final value) */
#endif
#define GB_SR 44100.0f
#define GB_ISR (1.0f / GB_SR)
#define GB_PI 3.14159265358979f
#define GB_TWO_PI 6.28318530717959f

#define GB_INLINE static inline __attribute__((always_inline))
#define GB_HOT                      /* (target: the section for code kept hot; empty on the PC) */
#if defined(__PI32V2__)
#define GB_LOWRAM __attribute__((section(".lowbss")))   /* RAM below the firmware (fw/ram.ld): written before read */
#define GB_BOOTRAM __attribute__((section(".bootbss")))  /* m-bootloader's RAM, ours once we run: written before read */
#else
#define GB_LOWRAM
#define GB_BOOTRAM
#endif
#if defined(__clang__)
#define GB_SMALL __attribute__((minsize))   /* code run seldom (a note's start, a page): small rather than fast */
#else
#define GB_SMALL
#endif
/* unroll a sample loop by 4 (or 2 where 4 would spill: 16 registers for integers and floats together): a taken
 * branch costs 6 cycles on the FM-1, three float operations (docs/MESURES-FM1.md) */
#if defined(__clang__)
#define GB_UNROLL4 _Pragma("clang loop unroll_count(4)")
#define GB_UNROLL2 _Pragma("clang loop unroll_count(2)")
#elif defined(__GNUC__)
#define GB_UNROLL4 _Pragma("GCC unroll 4")
#define GB_UNROLL2 _Pragma("GCC unroll 2")
#else
#define GB_UNROLL4
#define GB_UNROLL2
#endif

#if (GB_BLOCK & (GB_BLOCK - 1)) != 0 || GB_BLOCK < 16 || GB_BLOCK > 256
#error "GB_BLOCK: a power of two, 16..256"
#endif

GB_INLINE float gb_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
GB_INLINE int gb_clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
GB_INLINE float gb_minf(float a, float b) { return a < b ? a : b; }
GB_INLINE float gb_maxf(float a, float b) { return a > b ? a : b; }
GB_INLINE float gb_absf(float x) { return __builtin_fabsf(x); }                  /* a mask: one integer cycle */

/* 2^x for |x| < 32, Taylor to the 8th power on the fraction (relative error < 2e-7): the pitch path in float, so every
 * note 0..127 is higher than the one before (G8). No libm: the target's libm is not the PC's (OpenPocket's lesson). */
GB_INLINE float gb_exp2f(float x)
{
    if (x < -31.0f) return 0.0f;
    if (x > 31.0f) x = 31.0f;
    int i = (int)x; if ((float)i > x) i--;          /* floor */
    float f = x - (float)i;                         /* 0..1 */
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * (0.00961813f + f * (0.00133336f +
              f * (0.00015404f + f * (0.00001525f + f * 0.00000132f)))))));
    union { float f; uint32_t u; } v; v.f = p;
    v.u += (uint32_t)i << 23;                       /* scale by 2^i through the exponent */
    return v.f;
}
/* the frequency of a MIDI note (fractional), A4 = 440 Hz */
GB_INLINE float gb_note_hz(float note) { return 440.0f * gb_exp2f((note - 69.0f) * (1.0f / 12.0f)); }
/* decibels to gain */
GB_INLINE float gb_db(float db) { return gb_exp2f(db * 0.16609640f); }   /* 10^(db/20) = 2^(db * log2(10)/20) */

/* sin over one turn, t in [0, 1): Taylor to the 9th power on the quarter wave (error < 4e-6) */
GB_INLINE float gb_sin_turn(float t)
{
    t -= (float)(int)t; if (t < 0.0f) t += 1.0f;
    float x = t < 0.5f ? t : t - 0.5f, s = t < 0.5f ? 1.0f : -1.0f;  /* half wave */
    x = x < 0.25f ? x : 0.5f - x;                                    /* quarter wave, 0..0.25 */
    float z = x * 4.0f, z2 = z * z;                                  /* 0..1 */
    return s * z * (1.5707963f + z2 * (-0.6459641f + z2 * (0.0796926f + z2 * (-0.0046817f + z2 * 0.0001604f))));
}
/* a square root (x >= 0): a guess from the exponent, three Newton steps (no libm on the FM-1) */
GB_INLINE float gb_sqrtf(float x)
{
    if (x <= 0.0f) return 0.0f;
    union { float f; uint32_t u; } g; g.f = x; g.u = (g.u >> 1) + 0x1FC00000u;
    float y = g.f;
    y = 0.5f * (y + x / y); y = 0.5f * (y + x / y); y = 0.5f * (y + x / y);
    return y;
}
/* a soft saturation, odd, smooth, |y| < 1: x / sqrt(1 + x^2) through a rational (no division by zero) */
GB_INLINE float gb_sat(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);                   /* tanh's Pade, exact at +-3 */
}
/* white noise: xorshift32 to [-1, 1) */
GB_INLINE float gb_noise(uint32_t *s)
{
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}
/* a tiny alternating offset added where feedback can fall into denormals (OpenPocket's lesson) */
#define GB_ANTIDENORMAL 1e-18f

/* time: samples since power-on, 64 bits; deadlines compared by difference (G10) */
typedef uint64_t gb_time;
GB_INLINE int gb_time_before(gb_time a, gb_time b) { return (int64_t)(a - b) < 0; }
