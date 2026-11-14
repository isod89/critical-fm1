/* SPDX-License-Identifier: GPL-3.0-only
 * The firmware's first instruction, at 0x02000120 in the flash window (XIP), where m-bootloader calls it with
 * r0 = its boot object (SPEC 6.5), interrupts and watchdog off, on its stack. We copy the firmware (linked for RAM
 * at 0x01C30000, as the RAM apps) from the flash window to RAM and run it there: the engine was measured in RAM,
 * and nothing then waits on the flash. If it returns (HOME held), we return too: m-bootloader opens its menu. */
	.section .stub,"ax"
	.globl _xip
_xip:
#ifndef XSUM
	[--sp] = rets
	r1 = __payload_start
	r2 = 0x01C30000
	r3 = __payload_words
1:	r0 = [r1++=4]
	[r2++=4] = r0
	if (--r3 != 0) goto 1b
	csync
#else
	/* BOOT_DIAG: the copy summed word by word against the sum of ram.bin (fw/build.sh). Different: the window read
	 * wrong, and instead of running a damaged copy the backlight (PA2, low = lit) blinks slowly, forever */
	[--sp] = {rets, r5-r4}
	r1 = __payload_start
	r2 = 0x01C30000
	r3 = __payload_words
	r4 = 0
1:	r0 = [r1++=4]
	[r2++=4] = r0
	r4 = r4 + r0
	if (--r3 != 0) goto 1b
	csync
	r5 = XSUM
	if (r4 == r5) goto 4f
	r1 = 0x50000
	r0 = [r1+8]
	r2 = -5
	r0 &= r2
	[r1+8] = r0
	r3 = 4
2:	r0 = [r1+0]
	r0 ^= r3
	[r1+0] = r0
	r5 = 120000000
3:	if (--r5 != 0) goto 3b
	goto 2b
4:	{rets, r5-r4} = [sp++]
	[--sp] = rets
#endif
	r1 = 0x01C30000
	r0 = 60
	call r1
	pc = [sp++]

	.section .xcode,"ax"
	.incbin XCODE

	.section .payload,"a"
	.balign 4
	.globl __payload_start
__payload_start:
	.incbin PAYLOAD
	.balign 4
	.globl __payload_end
__payload_end:
