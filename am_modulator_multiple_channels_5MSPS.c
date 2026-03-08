// Kompilieren: gcc -O3 -march=native am_modulator_multiple_channels_5MSPS.c -o am_modulator_5MSPS -lm -lpthread -lliquid
// Aufruf: ./am_modulator 1234:603000 1235:828000 1236:999000
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
#include <liquid/liquid.h>

#define INTER_RATE   5000000.0f 
#define RING_SIZE    1048576
#define AUDIO_CHUNK  500  

typedef struct {
    int sock;
    uint8_t ring[RING_SIZE];
    volatile uint32_t head, tail;
    msresamp_rrrf resampler; // schneller als one-pass resamp_rrrf_create(200.0f, 20, 0.18f, 60.0f, 64);
    nco_crcf vco;
    float last_min_in, last_max_in;
    float res_puffer[120000];
    unsigned int last_nw;
    float gain;          
    float peak_hold;
    float freq;
    float external_gain;
    int port;
} Channel;

// Thread für Modulationssigal
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
typedef struct {
    Channel *channels;
    int num_transmitters;
} ControlData;

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
                    if (fabsf(cd->channels[i].freq - f_val) < 1.0f) { // Frequenz-Match
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

    int num_transmitters = argc - 1;
    //Channel channels[TRANSMITTER]; -> dynamisch reservieren auf HEAP, denn ab 7 Sendern > 8192kB Stacksize nicht ausreichend (Seg. fault)
    Channel *channels = malloc(sizeof(Channel) * num_transmitters);
    if (!channels) { perror("malloc"); return 1; }

    for (int i = 0; i < num_transmitters; i++) {
        // Parsing "Port:Frequenz"
        char *arg_copy = strdup(argv[i+1]);
        char *p_str = strtok(arg_copy, ":");
        char *f_str = strtok(NULL, ":");
        
        if (!p_str || !f_str) {
            fprintf(stderr, "Fehler: Argument %d (%s) ungültig. Erwarte Port:Freq\n", i+1, argv[i+1]);
            return 1;
        }

        channels[i].port = atoi(p_str);
        channels[i].freq = atof(f_str);
		channels[i].external_gain = 1.0f;
        free(arg_copy);

        // Socket Setup
        channels[i].sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr = { .sin_family=AF_INET, .sin_port=htons(channels[i].port), .sin_addr.s_addr=INADDR_ANY };
        if (bind(channels[i].sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            fprintf(stderr, "Konnte Port %d nicht binden.\n", channels[i].port);
            return 1;
        }
        fcntl(channels[i].sock, O_NONBLOCK);
        
        channels[i].head = channels[i].tail = 0;
        channels[i].gain = 1.0f;
        channels[i].peak_hold = 0.1f;
		channels[i].resampler = msresamp_rrrf_create(200.0f, 60.0f);  
        channels[i].vco = nco_crcf_create(LIQUID_VCO); // LIQUID_VCO = using LUT
        nco_crcf_set_frequency(channels[i].vco, 2.0f * (float)M_PI * channels[i].freq / INTER_RATE);

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

    float audio_in[AUDIO_CHUNK];
    uint8_t dac_out[120000 * 4]; 
    int debug_counter = 0;

    while(1) {
        unsigned int common_nw = 0;
        for (int i = 0; i < num_transmitters; i++) {
            channels[i].last_min_in = channels[i].last_max_in = 0.0f;
            float current_chunk_peak = 0.001f;

            if ((channels[i].head - channels[i].tail) >= AUDIO_CHUNK) {
                for (int j = 0; j < AUDIO_CHUNK; j++) {
                    float raw = ((float)channels[i].ring[channels[i].tail++ & (RING_SIZE - 1)] - 128.0f) / 128.0f;
                    if (fabsf(raw) > current_chunk_peak) current_chunk_peak = fabsf(raw);
                    audio_in[j] = raw * channels[i].gain;
                    if (audio_in[j] < channels[i].last_min_in) channels[i].last_min_in = audio_in[j];
                    if (audio_in[j] > channels[i].last_max_in) channels[i].last_max_in = audio_in[j];
                }
            } else { memset(audio_in, 0, sizeof(audio_in)); }

            // Sanfte AGC
            channels[i].peak_hold = 0.99f * channels[i].peak_hold + 0.01f * current_chunk_peak;
            float target_gain = 0.8f / (channels[i].peak_hold + 0.0001f);
            channels[i].gain = 0.998f * channels[i].gain + 0.002f * target_gain;
            if (channels[i].gain > 8.0f) channels[i].gain = 8.0f; 

            //resamp_rrrf_execute_block(channels[i].resampler, audio_in, AUDIO_CHUNK, channels[i].res_puffer, &channels[i].last_nw);
            msresamp_rrrf_execute(channels[i].resampler, audio_in, AUDIO_CHUNK, channels[i].res_puffer, &channels[i].last_nw);
            common_nw = channels[i].last_nw;
        }

        float dac_min = 1e9, dac_max = -1e9;
        for (int j = 0; j < common_nw; j++) {
            float sum = 0.0f;
            for (int i = 0; i < num_transmitters; i++) {
                float c = nco_crcf_cos(channels[i].vco); // nco cosinus for actual sample
                nco_crcf_step(channels[i].vco); // next nco step

		// SAFE SUMMATION:
                // Mod-Index 0.7f (verhindert Übermodulation)
                // Faktor 0.40f (gibt jedem Sender 40% (bei 2 TXs) vom DAC-Raum -> 80% Summe)
                float Faktor = 0.80f / num_transmitters;
				//  s(t) = Ac [ 1 + m(t)]*cos(2Pi*fc*t)   -> Ac = Ampl. des Trägers, m(t) = Modulationssignal, cos(2Pi...) = Träger        
                sum += (1.0f + channels[i].res_puffer[j] * 0.5f) * Faktor * c * channels[i].external_gain;
            }
            
	     // Skalierung auf Bereich (Headroom gegen Splatter)
            float val_f = sum * 60.0f; 

	     // Hard Limiters (Safety First)
            if (val_f > 127.0f) val_f = 127.0f;
            if (val_f < -128.0f) val_f = -128.0f;
            
            int8_t val = (int8_t)val_f;
            for (int k = 0; k < 2; k++) dac_out[j * 2 + k] = (uint8_t)val;
            
            if (val_f < dac_min) dac_min = val_f;
            if (val_f > dac_max) dac_max = val_f;
        }

        // Ausführliches Debug-Interface, Pegelanzeige der Eingangskanäle (AGC controlled) und DAC Aussteuerung
        if (++debug_counter > 25) {
            printf("\033[H\033[J--- AM MULTI-MODULATOR (%d Kanäle) ---\n", num_transmitters);
            for(int i=0; i < num_transmitters; i++) {
                float mod_val = (fabsf(channels[i].last_min_in) > channels[i].last_max_in) ? 
                                 fabsf(channels[i].last_min_in) : channels[i].last_max_in;
                printf("CH %d [%d]: Freq: %7.1f kHz Ext-Gain: %4.2f | (slow AGC) Gain: %4.1fx | Eff. Mod: %5.1f%%\n", 
                        i, channels[i].port, channels[i].freq/1000.0f, channels[i].external_gain, channels[i].gain, mod_val * 70.0f);
            }
            printf("----------------------------------------------------\n");
            printf("DAC Out (Signed): %6.1f / %6.1f (Safe: -128 bis 127)\n", dac_min, dac_max);
            debug_counter = 0;
        }

	// streaming HF/RF to port 12345
        if (common_nw > 0) send(client_fd, dac_out, common_nw * 2, 0);
    }
    return 0;
}
