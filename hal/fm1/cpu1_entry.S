/* SPDX-License-Identifier: GPL-3.0-only
 * CPU1's entry (pi32v2-isa docs/CPU1.md, step 6): out of the ROM's start-up context with interrupts off, no
 * interrupt configuration, its own user and supervisor stacks, into cpu1_main (cpu1.c), which never returns.
 * cpu1_start puts this address in the word CPU1 starts from (0x01C7FFF8). */
	.text
	.balign 4
	.globl cpu1_entry
cpu1_entry:
	cli
	icfg = 0
	usp = cpu1_ustack_top
	sp = cpu1_ustack_top
	ssp = cpu1_sstack_top
	reti = cpu1_main
	rti

	.bss
	.balign 8
	.globl cpu1_ustack
cpu1_ustack:			/* the jobs' stack: the engine's voices (a few KB) */
	.space 6144
	.globl cpu1_ustack_top
cpu1_ustack_top:
	.space 512		/* exceptions only: no interrupts on this core */
	.globl cpu1_sstack_top
cpu1_sstack_top:
