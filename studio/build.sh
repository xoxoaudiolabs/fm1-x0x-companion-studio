#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build x0x-companion-studio, X0X for the FM-1 Companion Studio, from this repo's X0X, on the Pi itself.
#   ./build.sh                  -> ./x0x-companion-studio
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device). Needs gcc and python3.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/gen
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
[ -f build/gen/x0x_drum_samples.h ] || python3 tools/gen_drum_samples.py build/gen/x0x_drum_samples.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
[ -f build/gen/x0x_break_bank.h ] || sh tools/gen_builtin_break.sh build/gen >/dev/null
U="firmware/src/dsp/drum909.c firmware/src/dsp/drum808.c firmware/src/dsp/bass303.c firmware/src/dsp/breaks.c
   firmware/src/dsp/fxbus.c firmware/src/dsp/master.c firmware/src/seq/sequencer.c firmware/src/seq/tb3po.c
   firmware/src/seq/pattern.c firmware/src/seq/motion.c firmware/src/app/engine.c"
# shellcheck disable=SC2086
gcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DX0X_HOST -DX0X_WEB -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    -o studio/x0x-companion-studio studio/x0x_studio.c $U -lm
echo "built studio/x0x-companion-studio"
