# AMWaveSynth
### Wave synthesizer for long- and mediumwave 
![alt text](https://github.com/radiolab81/AMWaveSynth/blob/main/www/AMWaveSynth_rnd.jpg "Logo Title Text 1")

The AMWaveSynth is an environment for generating multiple parallel running AM-modulated radiostations for transmission via an SDR.

The AMWaveSynth receives the modulation signal as PCM samples over UDP ports (1234 and above), the RF signal containing all radio stations is available as a data stream on port 12345 for further processing or transmission with an SDR transmitter such as the FL2K-device.

There are two versions of the modulator as a console program: one for a medium (NCO) samplerate of 2.5 MSPS (mainly for supplying longwave radio and RF wirecast receivers (Biennophone/TD-HF/Filodiffusione), the lower medium wave range is also reached) and a 5-MSPS (NCO) version for the entire frequency range up to 2.5 MHz.

The modulator is used as follows:

`Usage: ./am_modulator <port1:freq1> <port2:freq2> ...`

`Example: ./am_modulator 1234:603000 1235:828000`

Audiostream (8Bit,25kSPS) from port 1234 is AM modulated on 603kHz, a 2nd station with audio from port 1235 will be generated on 828 kHz....      

RF out is fixed on port 12345 with 10 MSPS and can be transmitted using fl2k_tcp, for example:

`fl2k_tcp -a 127.0.0.1 -p 12345 -s 10000000`

#### Attention: Variable rf bit-resolution (10, 12, 14, 16 bit) and sample rates (5, 10, 12.5, 25 MSPS) for other SDR transmitters are only available in the INT32 DSP version. If you prefer the way-faster 32-bit integer math version, simply call `./build_modulators_int.sh` and set USE_INT32_MATH_VERSION=true /amtxgui/start_sender.sh .

Audio modulation signal can be created with programs like ffmpeg:

`ffmpeg -re -i URL_station_1  -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1234 &`

`ffmpeg -re -i URL station_2  -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1235 &`


Additionally, there is a multilingual PythonTK user interface for controlling the entire transmission process. Transmitters can be configured by specifying the frequency, audio bandwidth, and playback source (local/remote URL).

![UI1](https://github.com/radiolab81/AMWaveSynth/blob/main/www/UI.jpg "Logo Title Text 1")

Stations can be aligned according to specific rf wave plans.

![UI2](https://github.com/radiolab81/AMWaveSynth/blob/main/www/UI2.jpg "Logo Title Text 1")

Predefined internet radio stations can be selected via a built-in station database (stations.db).

![UI3](https://github.com/radiolab81/AMWaveSynth/blob/main/www/UI3.jpg "Logo Title Text 1")

Multiple complete configured broadcasting landscapes can be mapped as a CSV file, imported and exported.

![UI4](https://github.com/radiolab81/AMWaveSynth/blob/main/www/modulator_debug.jpg "Logo Title Text 1")

Audio source gain (AGC controlled) and RF-DAC saturation is visible in the modulator console during transmission. The modulator can be easily switched to 16-bit audio and 10-16 bit wide RF-DACs (see INT32-DSP version). This would easily enable transmissions via STEMLab, Adalm2000, and other DACs (R2R ladder) or to the smisdr project (https://github.com/radiolab81/smisdr)

## Benchmarks 

### (liquid-dsp version):    `./build_modulators.sh`

| CPU/RAM       | 2.5MSPS LW/lower BC band | 5MSPS full 2.5MHz rf spectrum  |
| ------------- |:-------------:| -----:|
| Core2Duo/8GB  | 8-10 live stations | 5-6 live stations |
| Core3i 1st gen/4GB| 11-13 live stations  |  8-10 live stations |

Update: In addition to the liquid-dsp version, there is now a 2.5/5 MSPS version that primarily works with 32-bit integers, providing a significant boost in terms of the number of radio stations that can be created, even on older CPUs ! This version also supports various sampling rates and bit resolutions.

### (just 32bit Integer - version): `./build_modulators_int.sh`
| CPU/RAM       | 2.5MSPS LW/lower BC band | 5MSPS full 2.5MHz rf spectrum  |
| ------------- |:-------------:| -----:|
| Core2Duo/8GB  | 20+ no problems ...| <---|
| Core3i 1st gen/4GB| uncountable ;-) ...| <--- |

![PERF32](https://github.com/radiolab81/AMWaveSynth/blob/main/www/int32versionperf.jpg "Logo Title Text 1")

An entire long and medium wave band, generated in real-time on Intel Core2Duo and transmitted by osmo-fl2k, with almost no cost to the CPU.

![PERF322](https://github.com/radiolab81/AMWaveSynth/blob/main/www/int32versionperf2.jpg "Logo Title Text 1")
![PERF323](https://github.com/radiolab81/AMWaveSynth/blob/main/www/cpu.jpg "Logo Title Text 1")


#### You can also use add-ons like the AMWaveSynthPropagationSimulator https://github.com/radiolab81/AMWaveSynthPropagationSimulator to externally modify the carriers of the AMWaveSynth, for example to simulate day and night propagation or grayline transitions on a real radio.

![UI5](https://github.com/radiolab81/AMWaveSynthPropagationSimulator/raw/main/www/mw_dusk_01.jpg "Logo Title Text 1")

## Prerequisite for compiling on Debian 12/13

build-essential 

cmake

git

libusb-1*-dev 

bc          (for amtxgui)

python3-tk  (for amtxgui)


liquiddsp and osmo-fl2k (are already in the lib directory and can be built with the associated scripts)

Please make all shell scripts executable in project dir, amtxgui and lib using `chmod +x *.sh`, if this is not already the case on your system.
Should you require root privileges to run programs like fl2k_, create the necessary exception rule or start the application with root privileges by yourself.

```console
#!/bin/bash
sudo apt update
sudo apt install build-essential cmake git libusb-1.* bc python3-tk xterm ffmpeg
git clone https://github.com/radiolab81/AMWaveSynth
cd AMWaveSynth/libs
chmod +x *.sh
sudo ./build_osmo_fl2k.sh
sudo ./build_liquiddsp.sh
sudo ldconfig
cd ..
chmod +x *.sh
./build_modulators.sh
cd amtxgui
chmod +x *.sh
```
If you prefer the way-faster 32-bit integer math version, simply call `./build_modulators_int.sh` .

#### FAQ: - if modulator process terminates with an illegal machine instruction on your CPU, you must build liquiddsp with the option cmake -DENABLE_SIMD=OFF in the build_liquiddsp.sh 
