// Kompilieren: gcc -O3 -march=native am_modulator_5MSPS_integer.c -o am_modulator_5MSPS -lpthread -lm
// Aufruf: ./am_modulator_5MSPS 1234:603000 1235:828000
// sudo fl2k_tcp -a 127.0.0.1 -p 12345 -s 10000000

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define INTER_RATE   5000000.0f 
#define RING_SIZE    1048576
#define AUDIO_CHUNK  500  
#define UPSAMPLE_FAC 200     // 5.0 MHz / 25 kHz Audio Rate
#define LUT_BITS     12      // 12-Bit LUT = 4096 Einträge
#define LUT_SIZE     (1 << LUT_BITS)

// Globale NCO Sinus-Tabelle
int16_t sine_lut[LUT_SIZE];

typedef struct {
    int sock;
    uint8_t ring[RING_SIZE];
    volatile uint32_t head, tail;
    
    // NCO Parameter (32-Bit Phasenakkumulator)
    uint32_t phase;
    uint32_t phase_inc;
    
    // Modulations-Register
    int32_t mod_register;
    int32_t audio_buf[AUDIO_CHUNK];
    
    // Gain & AGC (Bleibt float, da nur in der langsamen Schleife berechnet)
    float last_min_in, last_max_in;
    float gain;          
    float peak_hold;
    float freq;
    float external_gain;
    int32_t total_gain_int; 
    int32_t carrier_base_int;
    
    int port;
} Channel;

// Thread für Audiosignale (UDP)
void* audio_receiver(void* arg) {
    Channel *ch = (Channel*)arg;
    uint8_t buf[4096];
    while(1) {
        ssize_t n = recv(ch->sock, buf, sizeof(buf), 0);
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                ch->ring[ch->head & (RING_SIZE - 1)] = buf[i];
                ch->head++;
            }
        } else { usleep(100); }
    }
    return NULL;
}

// Thread für die Steuersignale (Port 8888)
typedef struct { Channel *channels; int num_transmitters; } ControlData;

void* control_receiver(void* arg) {
    ControlData *cd = (ControlData*)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = { .sin_family=AF_INET, .sin_port=htons(8888), .sin_addr.s_addr=INADDR_ANY };
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    
    char buf[256];
    while(1) {
        ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n > 0) {
            buf[n] = '\0';
            float f_val, g_val;
            if (sscanf(buf, "%f:%f", &f_val, &g_val) == 2) {
                // Suche den passenden Kanal anhand der Frequenz
                for (int i = 0; i < cd->num_transmitters; i++) {
                    if (fabsf(cd->channels[i].freq - f_val) < 1.0f) {
                        cd->channels[i].external_gain = g_val;
                        break;
                    }
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Nutzung: %s <port1:freq1> <port2:freq2> ...\n", argv[0]);
        fprintf(stderr, "Beispiel: %s 1234:603000 1235:828000\n", argv[0]);
        return 1;
    }

    // 1. Initialisiere die NCO Lookup-Tabelle (einmalig beim Start)
    for(int i = 0; i < LUT_SIZE; i++) {
        sine_lut[i] = (int16_t)(round(sin(2.0 * M_PI * i / LUT_SIZE) * 32767.0));
    }

    int num_transmitters = argc - 1;
    Channel *channels = calloc(num_transmitters, sizeof(Channel));
    if (!channels) { perror("calloc"); return 1; }

    for (int i = 0; i < num_transmitters; i++) {
        // Parsing "Port:Frequenz"
        char *arg_copy = strdup(argv[i+1]);
        char *p_str = strtok(arg_copy, ":");
        char *f_str = strtok(NULL, ":");
        
        channels[i].port = atoi(p_str);
        channels[i].freq = atof(f_str);
        channels[i].external_gain = 1.0f;
        free(arg_copy);

        // Socket Setup
        channels[i].sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr = { .sin_family=AF_INET, .sin_port=htons(channels[i].port), .sin_addr.s_addr=INADDR_ANY };
        bind(channels[i].sock, (struct sockaddr *)&addr, sizeof(addr));
        fcntl(channels[i].sock, O_NONBLOCK);
        
        channels[i].gain = 1.0f;
        channels[i].peak_hold = 0.1f;
        
        // Integer NCO Phase Increment: (Freq / 5.0 MSPS) * 2^32
        double fraction = channels[i].freq / INTER_RATE;
        channels[i].phase_inc = (uint32_t)(fraction * 4294967296.0);
        channels[i].phase = 0;

        pthread_t tid;
        pthread_create(&tid, NULL, audio_receiver, &channels[i]);
    }

    // Starte den Control-Receiver
    ControlData cd = { channels, num_transmitters };
    pthread_t ctid;
    pthread_create(&ctid, NULL, control_receiver, &cd);

    // TCP Setup für fl2k_tcp 
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in s_addr = { .sin_family=AF_INET, .sin_port=htons(12345), .sin_addr.s_addr=INADDR_ANY };
    bind(listen_fd, (struct sockaddr *)&s_addr, sizeof(s_addr));
    listen(listen_fd, 1);
    
    printf("Warte auf fl2k_tcp (10MSPS) an Port 12345...\n");
    int client_fd = accept(listen_fd, NULL, NULL);

    // Buffer-Größe: 500 (Audio) * 200 (Upsample) * 2 (Ausgabe-Verdopplung für 10 MSPS)
    uint8_t dac_out[AUDIO_CHUNK * UPSAMPLE_FAC * 2]; 
    int debug_counter = 0;
    
    // Skalierungsfaktor für den DAC. Verhindert Übersteuerung bei der Summierung.
    // Ein höherer Wert macht das Gesamtsignal leiser.
    int32_t dac_divisor = 230 * num_transmitters; 

    while(1) {
        // ====================================================================
        // LANGSAME SCHLEIFE (AUDIO RATE)
        // ====================================================================
        for (int i = 0; i < num_transmitters; i++) {
            // Wir setzen die Werte NICHT mehr hart auf 0, 
            // sondern nur, wenn wir Daten haben.
            float current_chunk_peak = 0.0f; 
            int data_present = 0;

            if ((channels[i].head - channels[i].tail) >= AUDIO_CHUNK) {
                data_present = 1;
                // Lokale Min/Max für diesen EINEN Block
                float block_min = 0.0f;
                float block_max = 0.0f;

                for (int j = 0; j < AUDIO_CHUNK; j++) {
                    // Wandle 8-Bit unsigned auf signed int32_t (-128 bis 127)
                    int32_t raw_val = (int32_t)channels[i].ring[channels[i].tail++ & (RING_SIZE - 1)] - 128;
                    channels[i].audio_buf[j] = raw_val;
                    
                    float raw_f = (float)raw_val / 128.0f;
                    if (fabsf(raw_f) > current_chunk_peak) current_chunk_peak = fabsf(raw_f);
                    if (raw_f < block_min) block_min = raw_f;
                    if (raw_f > block_max) block_max = raw_f;
                }
                // Nur bei echten Daten die "Anzeige-Variablen" aktualisieren
                channels[i].last_min_in = block_min;
                channels[i].last_max_in = block_max;
            } else { memset(channels[i].audio_buf, 0, sizeof(channels[i].audio_buf)); }

            // AGC nur nachführen, wenn Daten da waren oder das Signal zu laut war
            // Das verhindert das "Hochlaufen" des Gains in Pausen (Gating)
            if (data_present || current_chunk_peak > channels[i].peak_hold) {
                channels[i].peak_hold = 0.995f * channels[i].peak_hold + 0.005f * current_chunk_peak;
                float target_gain = 0.8f / (channels[i].peak_hold + 0.001f);
                channels[i].gain = 0.998f * channels[i].gain + 0.002f * target_gain;
                if (channels[i].gain > 8.0f) channels[i].gain = 8.0f; 
                if (channels[i].gain < 0.1f) channels[i].gain = 0.1f;
            }

            // Gain für Integer-Mathe vorberechnen (Mod-Index 0.5 aus deinem Code entspricht Faktor 64.0f)
            channels[i].total_gain_int = (int32_t)(channels[i].gain * channels[i].external_gain * 64.0f);
            channels[i].carrier_base_int = (int32_t)(16384.0f * channels[i].external_gain);
        }

        // ====================================================================
        // SCHNELLE SCHLEIFE (RF RATE) - Sample & Hold Architektur
        // ====================================================================
        unsigned int out_idx = 0;
        int32_t dac_min_int = 1000, dac_max_int = -1000;
        
        for (int a = 0; a < AUDIO_CHUNK; a++) {
            
            // Modulationsregister updaten
            for (int i = 0; i < num_transmitters; i++) {
                // Träger-Basisamplitude (16384) + moduliertes Signal
                channels[i].mod_register = channels[i].carrier_base_int + (channels[i].audio_buf[a] * channels[i].total_gain_int);
            }

            // Halten für 200 HF-Samples (5 MSPS)
            for (int u = 0; u < UPSAMPLE_FAC; u++) {
                int32_t mix = 0;
                
                for (int i = 0; i < num_transmitters; i++) {
                    // NCO Schritt
                    channels[i].phase += channels[i].phase_inc;
                    
                    // Träger aus LUT holen (Die oberen 12 Bit der Phase als Index)
                    int32_t carrier = sine_lut[channels[i].phase >> (32 - LUT_BITS)];
                    
                    // Amplituden-Modulation: Träger * (Basis + Audio)
                    // int64_t Cast verhindert Überlauf bei sehr hohem Gain, Shift >> 15 skaliert zurück
                    int32_t modulated = (int32_t)(((int64_t)channels[i].mod_register * carrier) >> 15);
                    
                    mix += modulated;
                }
                
                // 32-Bit Mix auf 8-Bit DAC Bereich (-128 bis 127) skalieren
                int32_t dac_val = mix / dac_divisor;

                // Hard Limiters
                if (dac_val > 127) dac_val = 127;
                if (dac_val < -128) dac_val = -128;
                
                if (dac_val < dac_min_int) dac_min_int = dac_val;
                if (dac_val > dac_max_int) dac_max_int = dac_val;

                // Verdopplung auf 10 MSPS (DAC Output)
                for (int k = 0; k < 2; k++) {
                    dac_out[out_idx++] = (uint8_t)(int8_t)dac_val;
                }
            }
        }

        // Debug & Stats
        if (++debug_counter > 25) {
            printf("\033[H\033[J--- AM 32-BIT INT-MODULATOR (%d Kanäle) ---\n", num_transmitters);
            for(int i=0; i < num_transmitters; i++) {
                float mod_val = (fabsf(channels[i].last_min_in) > channels[i].last_max_in) ? 
                                 fabsf(channels[i].last_min_in) : channels[i].last_max_in;
                printf("CH %d [%d]: Freq: %7.1f kHz Ext-Gain: %4.2f | Gain: %4.1fx | Eff. Mod: %5.1f%%\n", 
                        i, channels[i].port, channels[i].freq/1000.0f, channels[i].external_gain, channels[i].gain, mod_val * 50.0f);
            }
            printf("----------------------------------------------------\n");
            printf("DAC Out (Signed): %6d / %6d (Safe: -128 bis 127)\n", dac_min_int, dac_max_int);
            debug_counter = 0;
        }

        // Output Index entspricht genau: 500 * 200 * 2 = 200.000 Bytes
        send(client_fd, dac_out, out_idx, 0);
    }
    return 0;
}
