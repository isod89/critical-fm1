/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL in RAM (as SLOOP 3's fw/ram.ld, without code in the flash window): the image in the RAM apps' slot from
 * 0x01C30000 (code, data, .bss), the sound's lines and tables below it (.lowbss: written before read), and
 * m-bootloader's own RAM once we run (.bootbss: buffers written before read). */
ENTRY(_start)
MEMORY {
  boot (rw)  : ORIGIN = 0x01C02000, LENGTH = 0x9000
  low  (rw)  : ORIGIN = 0x01C0B000, LENGTH = 0x25000
  slot (rwx) : ORIGIN = 0x01C30000, LENGTH = 0x4FD80
}
SECTIONS {
  .entry : { KEEP(*(.entry)) } > slot
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > slot
  .data : { *(.data*) *(.sdata*) . = ALIGN(4); } > slot
  __image_end = .;
  .bss (NOLOAD) : { . = ALIGN(8); __bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(8); __bss_end = .; } > slot
  .lowbss (NOLOAD) : { . = ALIGN(8); __lowbss_start = .; *(.lowbss*) . = ALIGN(8); __lowbss_end = .; } > low
  .stack (NOLOAD) : { . = ALIGN(8); __stack_low = .; . += 0x2000; __stack_top = .; } > low
  .bootbss (NOLOAD) : { . = ALIGN(8); *(.bootbss*) } > boot
  ASSERT(__bss_end <= 0x01C7FD80, "the firmware does not fit its slot")
}
