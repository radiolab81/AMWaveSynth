#!/usr/bin/env python3
import socket
import time
import random
import argparse

# --- ITU / CCIR inspirierte Sferics-Profile ---
# Ein Blitz (Flash) besteht oft aus mehreren Teilentladungen (Strokes).
PROFILES = {
    "1": {
        "name": "Quiet (Mid-Latitude / Winter)",
        "desc": "Seltene, weit entfernte Blitze. Sehr ruhiges Band.",
        "flash_interval_sec": (3.0, 8.0),   # Lange Pausen zwischen Blitzen
        "strokes_per_flash": (1, 2),        # Meist nur Einzelentladungen
        "stroke_pause_ms": (20, 50),
        "amplitude": (100, 300),            # Relativ leise
        "decay_ms": (5.0, 15.0)             # Kurzes Knistern
    },
    "2": {
        "name": "Moderate (Typischer Sommerabend)",
        "desc": "Gelegentliches, mittleres Rumpeln. Typisch für AM-Rundfunk.",
        "flash_interval_sec": (1.0, 3.5),
        "strokes_per_flash": (1, 4),        # Mehrere Teilentladungen
        "stroke_pause_ms": (30, 80),
        "amplitude": (300, 700),
        "decay_ms": (10.0, 25.0)
    },
    "3": {
        "name": "Poor (Tropical / Hohe Gewitteraktivität)",
        "desc": "Starkes, häufiges Krachen. Stört den Empfang merklich.",
        "flash_interval_sec": (0.3, 1.2),
        "strokes_per_flash": (2, 5),
        "stroke_pause_ms": (20, 60),
        "amplitude": (600, 1200),           # Sehr laut
        "decay_ms": (15.0, 40.0)            # Längeres "Nachhallen" im Äther
    },
    "4": {
        "name": "Severe (Lokales Gewitter / Nahbereich)",
        "desc": "Extrem lautes, fast durchgehendes Prasseln. Cluster-Strokes.",
        "flash_interval_sec": (0.1, 0.5),
        "strokes_per_flash": (3, 8),        # Komplexe Entladungskaskaden
        "stroke_pause_ms": (10, 40),
        "amplitude": (1000, 2500),          # Clipping-Gefahr (gewollt!)
        "decay_ms": (20.0, 60.0)
    }
}

def send_sferic(sock, ip, port, amp, decay):
    """Sendet exakt das Format, das C-Modulator erwartet."""
    msg = f"{amp:.1f}:{decay:.1f}".encode('utf-8')
    sock.sendto(msg, (ip, port))

def main():
    parser = argparse.ArgumentParser(description="ITU Sferics/Lightning Generator für AM Modulator")
    parser.add_argument("-i", "--ip", default="127.0.0.1", help="Ziel-IP des Modulators (Default: 127.0.0.1)")
    parser.add_argument("-p", "--port", type=int, default=8889, help="Ziel-UDP-Port des Modulators (Default: 8889)")
    parser.add_argument("-m", "--mode", choices=["1", "2", "3", "4"], help="Profil direkt auswählen")
    args = parser.parse_args()

    print("=== SFERICS GENERATOR ===")
    for key, prof in PROFILES.items():
        print(f"[{key}] {prof['name']} - {prof['desc']}")
    
    choice = args.mode
    if not choice:
        choice = input("\nBitte Profil wählen (1-4): ")
        if choice not in PROFILES:
            print("Ungültige Wahl. Starte mit Profil 2 (Moderate).")
            choice = "2"

    active_profile = PROFILES[choice]
    print(f"\nStarte Simulation: {active_profile['name']}")
    print(f"Sende UDP-Pakete an {args.ip}:{args.port} (Abbruch mit CTRL+C)\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        flash_count = 0
        while True:
            # 1. Bestimme, wann der nächste Hauptblitz (Flash) auftritt
            wait_sec = random.uniform(*active_profile["flash_interval_sec"])
            time.sleep(wait_sec)
            
            # 2. Bestimme, aus wie vielen Teilentladungen (Strokes) dieser Blitz besteht
            strokes = random.randint(*active_profile["strokes_per_flash"])
            
            print(f"Gewitter-Ereignis #{flash_count}: {strokes} Entladungen (Nächster in {wait_sec:.1f}s)")
            
            for s in range(strokes):
                # Generiere leichte Variationen für jeden Stroke innerhalb des Flashes
                amp = random.uniform(*active_profile["amplitude"])
                # Spätere Strokes in einem Flash sind meist etwas schwächer
                if s > 0: amp *= random.uniform(0.5, 0.9) 
                
                decay = random.uniform(*active_profile["decay_ms"])
                
                # Sende das Kommando an den C-Modulator
                send_sferic(sock, args.ip, args.port, amp, decay)
                
                # Kurze Pause bis zur nächsten Teilentladung (sofern es nicht die letzte ist)
                if s < strokes - 1:
                    pause_ms = random.uniform(*active_profile["stroke_pause_ms"])
                    time.sleep(pause_ms / 1000.0)
                    
            flash_count += 1

    except KeyboardInterrupt:
        print("\nSimulation beendet.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()