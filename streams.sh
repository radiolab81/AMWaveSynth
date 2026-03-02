ffmpeg -re -i https://tuner.m1.fm/80er.mp3   -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1234 &
ffmpeg -re -i https://tuner.m1.fm/90er.mp3   -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1235 &
ffmpeg -re -i http://live-icy.gss.dr.dk:8000/A/A04H.mp3   -af "lowpass=f=4500, volume=0.8, acompressor=threshold=-10dB:ratio=4" -f u8 -ar 25000 -ac 1 udp://127.0.0.1:1236 &
