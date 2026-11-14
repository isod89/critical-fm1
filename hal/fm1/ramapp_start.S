/* SPDX-License-Identifier: GPL-3.0-only
 * The entry of a RAM app (m-bootloader SPEC 11.8): called like a C function with r0 = the LSB clock in MHz,
 * interrupts and watchdog off, on the caller's stack. We clear our .bss (the trailer the loader wrote lands
 * there), move to our own stack, run app_main(r0), and come back on the caller's stack: the bootloader then
 * sets up flash, the panel and USB again. */
	.section .entry,"ax"
	.globl _start
_start:
	[--sp] = {rets, r6-r4}
	r6 = r0
	r1 = __bss_start
	r2 = __bss_end
	r3 = 0
1:	if (r1 >= r2) goto 2f
	[r1+0] = r3
	r1 = r1 + 4
	goto 1b
2:	r4 = sp
	r1 = __caller_sp
	[r1+0] = r4
	sp = __stack_top
	r0 = r6
	call app_main
	r1 = __caller_sp
	r4 = [r1+0]
	sp = r4
	{rets, r6-r4} = [sp++]
	rts

	.section .data
	.balign 4			/* (a word: ".align 2" here is 2 bytes, and a word stored there faults) */
__caller_sp:
	.long 0
