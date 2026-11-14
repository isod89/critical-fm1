/* SPDX-License-Identifier: GPL-3.0-only
 * The few C library routines the firmware needs (the FM-1 has no C library). Word-wide when aligned. */
#include "gb.h"

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = (uint8_t *)d;
    uint32_t w = (uint8_t)c; w |= w << 8; w |= w << 16;
    while (n && ((uintptr_t)p & 3)) { *p++ = (uint8_t)c; n--; }
    uint32_t *q = (uint32_t *)p;
    for (; n >= 16; n -= 16) { q[0] = w; q[1] = w; q[2] = w; q[3] = w; q += 4; }
    for (; n >= 4; n -= 4) *q++ = w;
    p = (uint8_t *)q;
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
    if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) {
        uint32_t *x = (uint32_t *)a; const uint32_t *y = (const uint32_t *)b;
        for (; n >= 16; n -= 16) { x[0] = y[0]; x[1] = y[1]; x[2] = y[2]; x[3] = y[3]; x += 4; y += 4; }
        for (; n >= 4; n -= 4) *x++ = *y++;
        a = (uint8_t *)x; b = (const uint8_t *)y;
    }
    while (n--) *a++ = *b++;
    return d;
}
void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
    if (a == b || n == 0) return d;
    if (a < b || a >= b + n) return memcpy(d, s, n);
    while (n--) a[n] = b[n];
    return d;
}
int memcmp(const void *x, const void *y, size_t n)
{
    const uint8_t *a = (const uint8_t *)x, *b = (const uint8_t *)y;
    for (; n; n--, a++, b++) if (*a != *b) return *a < *b ? -1 : 1;
    return 0;
}
size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
