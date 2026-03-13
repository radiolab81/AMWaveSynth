#!/bin/bash
apt update
apt install cmake
unzip liquid-dsp-1.7.0.zip
cd liquid-dsp-1.7.0
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig
