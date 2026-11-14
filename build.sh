#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# CRITICAL 4 on the PC: the hands' ways checked (flow3), a piece played as hands would (build/pc/play3.wav), a restless musician for half an hour
# three times over (soak3), the screens; core4 the reactor alone, a scripted session (core4 OUT.wav [VOID] [AZ_AT] [FUEL]); kinds3 renders every kind of every sound alone (kinds3 OUTDIR). The firmware itself: fw/build.sh.
set -e
cd "$(dirname "$0")"
O=build/pc; mkdir -p $O/shots3
CF="-O2 -std=c11 -Wall -Wextra -Wno-unused-parameter -D_DEFAULT_SOURCE"
S="src/inst.c src/synth.c"
cc $CF -o $O/play3 tests/play3.c $S -lm
cc $CF -o $O/soak3 tests/soak3.c $S -lm
cc $CF -o $O/kinds3 tests/kinds3.c $S -lm
cc $CF -o $O/core4 tests/core4.c $S -lm
cc $CF -o $O/flow3 tests/flow3.c $S -lm
cc $CF -o $O/film4 tests/film4.c $S src/screen.c app/gfx.c app/fonts.c -lm
cc $CF -o $O/shots3_bin tests/shots3.c $S src/screen.c app/gfx.c app/fonts.c -lm
$O/flow3
$O/play3 $O/play3.wav
for s in 1 2 3; do $O/soak3 ${SOAK_MIN:-30} $s; done
$O/shots3_bin $O/shots3
