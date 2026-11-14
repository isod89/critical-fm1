#!/bin/sh
# CRITICAL for the FM-1: build/fw/critical.area (the application area the installer page encrypts and installs
# through m-bootloader's MIDI MODE) and build/fw/installer-critical-VERSION.html.   fw/build.sh [LLVM_BIN]
# The compiler: the pi32v2 LLVM toolchain, release pi32v2-23.1.5-2 (https://github.com/jvitkauskas/pi32v2-llvm),
# its bin/ given here, or in PI32V2_LLVM, or unpacked in toolchain/ (see README.md).
set -eu
cd "$(dirname "$0")/.."
TC=${1:-${PI32V2_LLVM:-toolchain/bin}}
test -x "$TC/clang" || { echo "no pi32v2 clang in $TC: see README.md, Building"; exit 1; }
VERSION=$(sed -n 's/^#define VERSION "\(.*\)"/\1/p' fw/main.c)
O=build/fw; rm -rf $O; mkdir -p $O/o
CF="-mcpu=ac791n -DCRITICAL_FW -O2 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter -Wdouble-promotion"
for f in src/synth.c core/libc.c hal/fm1/audio.c hal/fm1/cpu1.c; do
  "$TC/clang" $CF -c $f -o $O/o/$(basename $f .c).o
done
for f in src/inst.c src/screen.c app/gfx.c app/fonts.c hal/fm1/board.c hal/fm1/panel.c hal/fm1/adc.c hal/fm1/flash.c fw/main.c; do
  "$TC/clang" $(echo "$CF" | sed 's/-O2/-Os/') -c $f -o $O/o/$(basename $f .c).o
done
"$TC/clang" -mcpu=ac791n -c hal/fm1/ramapp_start.S -o $O/o/start.o
"$TC/clang" -mcpu=ac791n -c hal/fm1/cpu1_entry.S -o $O/o/cpu1_entry.o
RT=$(dirname "$TC")/lib/clang/23/lib/pi32v2/libclang_rt.builtins.a
"$TC/ld.lld" -T fw/ram.ld --gc-sections -o $O/ram.elf $O/o/*.o "$RT"
"$TC/llvm-objcopy" -O binary --only-section=.entry --only-section=.text --only-section=.data $O/ram.elf $O/ram.bin
: > $O/xcode.bin
# the entry's own word on a word (SLOOP 3.0's lesson: a word stored off one faults at the very start)
"$TC/llvm-nm" $O/ram.elf | python3 -c "import sys; a=[int(l.split()[0],16) for l in sys.stdin if l.split()[-1]=='__caller_sp']; sys.exit('__caller_sp is not on a word' if not a or a[0]%4 else 0)"
# no soft-float or library call may hide in the image
"$TC/llvm-nm" -u $O/ram.elf | grep -v '^ *$' && { echo "undefined symbols"; exit 1; } || true
"$TC/clang" -mcpu=ac791n -DPAYLOAD="\"$O/ram.bin\"" -DXCODE="\"$O/xcode.bin\"" -c fw/xip.S -o $O/xip.o
"$TC/ld.lld" -T fw/xip.ld -o $O/app.elf $O/xip.o
"$TC/llvm-objcopy" -O binary $O/app.elf $O/app.bin
python3 tools/mkfw.py $O/app.bin $O/critical.area
# the area must end before the career's sector (0x77000), far from SLOOP 3's data (0x78000 on)
test $(stat -c%s $O/critical.area) -le $((0x77000 - 0x4000)) || { echo "the area would reach the career's sector"; exit 1; }
"$TC/llvm-size" -A $O/ram.elf | grep -E "^\.(entry|text|data|bss|lowbss|stack|bootbss) "
python3 tools/mkpage.py $O/critical.area "$VERSION" $O/installer-critical-$VERSION.html
