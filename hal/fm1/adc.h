/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's ADC (adc.c): the MASTER pot, polled. */
#pragma once
#include <stdint.h>
void adc_init(void);                     /* the pot's pin an analog input */
int32_t adc_read(uint32_t ch);           /* 0..1023, -1 if the converter did not answer (~0.2 ms) */
