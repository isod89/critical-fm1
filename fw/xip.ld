/* SPDX-License-Identifier: GPL-3.0-only
 * app.bin: the XIP stub at the entry m-bootloader calls (0x02000120, the app area's relative 0x120), then the
 * firmware's RAM image. */
ENTRY(_xip)
MEMORY { xip (rx) : ORIGIN = 0x02000120, LENGTH = 0x80000 }
SECTIONS {
  .stub : { KEEP(*(.stub)) } > xip
  ASSERT(. <= 0x02000400, "the stub runs into the window's code")
  .xcode 0x02000400 : { KEEP(*(.xcode)) } > xip                  /* the code run from the window (fw/ram.ld .xtext) */
  .payload : { . = ALIGN(4); KEEP(*(.payload)) } > xip
  __payload_words = (__payload_end - __payload_start) / 4;
}
