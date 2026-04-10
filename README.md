# AMWaveSynth
### Wave synthesizer for long- and mediumwave based on liquiddsp 
![alt text](https://github.com/radiolab81/AMWaveSynth/blob/main/www/AMWaveSynth_rnd.jpg "Logo Title Text 1")

The AMWaveSynth is an environment for generating multiple parallel running AM-modulated radiostations for transmission via an SDR.

The AMWaveSynth receives the modulation signal as PCM samples over UDP ports (1234 and above), the RF signal containing all radio stations is available as a data stream on port 12345 for further processing or transmission with an SDR transmitter such as the FL2K-device.

There are two versions of the modulator as a console program: one for a medium samplerate of 2.5 MSPS (mainly for supplying longwave radio and RF wirecast receivers (Biennophone/TD-HF/Filodiffusione), the lower medium wave range is also reached) and a 5-MSPS version for the entire frequency range up to 2.5 MHz.

The modulator is used as follows:

`Usage: ./am_modulator <port1:freq1> <port2:freq2> ...`

`Example: ./am_modulator 1234:603000 1235:828000`

Audiostream (8Bit,25kSPS) from port 1234 is AM modulated on 603kHz, a 2nd station with audio from port 1235 will be generated on 828 kHz....      

RF out is fixed on port 12345 with 10 MSPS and can be transmitted using fl2k_tcp, for example:

`fl2k_tcp -a 127.0.0.1 -p 12345 -s 10000000`

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

Audio source gain (AGC controlled) and RF-DAC saturation is visible in the modulator console during transmission. The modulator can be easily switched to 16-bit audio and 10-16 bit wide RF-DACs. This would easily enable transmissions via STEMLab, Adalm2000, and other DACs (R2R ladder).

## Benchmarks:

| CPU/RAM       | 2.5MSPS LW/lower BC band | 5MSPS full 2.5MHz rf spectrum  |
| ------------- |:-------------:| -----:|
| Core2Duo/8GB  | 8-10 live stations | 5-6 live stations |
| Core3i 1st gen/4GB| 11-13 live stations  |  8-10 live stations |


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

#### FAQ: - if modulator process terminates with an illegal machine instruction on your CPU, you must build liquiddsp with the option cmake -DENABLE_SIMD=OFF in the build_liquiddsp.sh 
