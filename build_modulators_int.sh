#!/bin/bash
gcc -O3 -march=native am_modulator_integer.c -o am_modulator -lpthread -lm
gcc -O3 -march=native am_modulator_5MSPS_integer.c -o am_modulator_5MSPS -lpthread -lm
cp am_modulator amtxgui
cp am_modulator_5MSPS amtxgui
[ -f amtxgui/start_sender.sh ] && sed -i 's/USE_INT32_MATH_VERSION=false/USE_INT32_MATH_VERSION=true/g' amtxgui/start_sender.sh
