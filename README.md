# AMWaveSynth
### wave synthesizer for long- and mediumwave based on liquiddsp 
![alt text](https://github.com/radiolab81/AMWaveSynth/blob/main/www/AMWaveSynth_rnd.jpg "Logo Title Text 1")

The AMWaveSynth is an environment for generating multiple parallel running am modulated radiostations for transmission via an SDR.

The AMWaveSynth receives the modulation signal as PCM samples via UDP ports (1234 and above), the RF signal containing all radio stations is available as a data stream on port 12345 for further processing or transmission with an SDR transmitter such as the FL2K.

There are two versions of the modulator as a console program: one for a medium sampling rate of 2.5 MSPS (mainly for supplying longwave radio and HF wirewave receivers, the lower medium wave range is also reached) and a 5-MSPS version for the entire frequency range up to 2.5 MHz.

The modulator is called via the console as follows:

`Usage: ./am_modulator <port1:freq1> <port2:freq2> ...`

`Example: ./am_modulator 1234:603000 1235:828000`

Audiostream (8Bit,25kSPS) from port 1234 is AM modulated on 603kHz, a 2nd station with audio from port 1235 will be generated on 828 kHz....      

RF out is on port 12345 with 10 MSPS and can be transmitted using fl2k_tcp, for example, as follows:

`fl2k_tcp -a 127.0.0.1 -p 12345 -s 10000000`

Audio modulation can be created with programs like ffmpeg:

`ffmpeg -re -i URL_station_1  -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1234 &`

`ffmpeg -re -i URL station_2  -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1235 &`
