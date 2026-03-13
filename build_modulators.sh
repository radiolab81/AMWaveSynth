#!/bin/bash
gcc -O3 -march=native am_modulator_multiple_channels.c -o am_modulator -lpthread -lliquid -lm
gcc -O3 -march=native am_modulator_multiple_channels_5MSPS.c -o am_modulator_5MSPS -lpthread -lliquid -lm
cp am_modulator amtxgui
cp am_modulator_5MSPS amtxgui
