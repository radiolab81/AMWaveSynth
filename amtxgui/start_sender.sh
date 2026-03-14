#!/bin/bash
echo "--- MULTI-SENDER START ---"

MOD_ARGS=""
USE_5MSPS=false  # Status-Variable initialisieren

# Wir verarbeiten die Argumente in 4er-Blöcken (Freq, BW, URL, Port)
while (( "$#" >= 4 )); do
    FREQ=$1
    BW=$(echo "$2 * 1000" | bc | cut -d'.' -f1)
    URL=$3
    PORT=$4
    
    # Prüfung: Wenn eine Frequenz > 1250 ist, Flag auf true setzen
    if [ "$FREQ" -gt 1250 ]; then
        USE_5MSPS=true
    fi

    echo "Starte ffmpeg Instanz: $FREQ kHz | BW: $BW kHz | Port: $PORT"
   
    # ffmpeg -i "$URL" -af "lowpass=f=${BW}000" -f mpegts udp://127.0.0.1:$PORT &
    ffmpeg -stream_loop -1 -re -i "$URL" -af "lowpass=f=${BW}, volume=0.8, acompressor=threshold=-10dB:ratio=4"   -f u8 -ar 25000 -ac 1 udp://127.0.0.1:$PORT &    
    
    # Den String für am_modulator zusammenbauen
    MOD_ARGS="$MOD_ARGS $PORT:$(($FREQ*1000))"

    shift 4 # Die nächsten 4 Parameter nehmen
done

# Entscheidung, welcher Modulator gestartet wird
if [ "$USE_5MSPS" = true ]; then
    echo "Frequenz > 1250 erkannt. Starte Modulator: ./am_modulator_5MSPS $MOD_ARGS"
    ./am_modulator_5MSPS $MOD_ARGS &
else
    echo "Alle Frequenzen <= 1250. Starte Modulator: ./am_modulator $MOD_ARGS"
    ./am_modulator $MOD_ARGS &
fi

echo "--------------------------"
echo "Alle Prozesse gestartet. Drücke STRG+C zum Beenden."
# Das Script muss aktiv bleiben, damit xterm offen bleibt
while true; do sleep 5; done
