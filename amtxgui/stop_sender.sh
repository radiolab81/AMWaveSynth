#!/bin/bash
# Schließt den Prozessbaum, der vom xterm (PID $1) gestartet wurde
#pkill -P $1
killall ffmpeg
killall am_modulator
killall am_modulator_5MSPS
killall socat
