#!/usr/bin/env python3
import subprocess
import re
import sys
import hashlib
import argparse
import base64
import os
import time

# Laut Matter Core Spec (Section 5.1.7.1) unzulässige Passcodes
RESTRICTED_PASSCODES = {
    0, 11111111, 22222222, 33333333, 44444444, 
    55555555, 66666666, 77777777, 88888888, 99999999, 
    12345678, 87654321
}

def get_esp_mac(port: str) -> str:
    """Liest die MAC-Adresse des ESP über esptool.py aus."""
    print(f"[*] Lese MAC-Adresse auf Port {port} via esptool.py...")
    try:
        t0 = time.time()
        result = subprocess.run(
            ["esptool.py", "--port", port, "read_mac"],
            capture_output=True, text=True, check=True
        )
        t1 = time.time()
        
        # Suche nach dem MAC-Muster im Output
        match = re.search(r"MAC:\s*([0-9a-fA-F:]+)", result.stdout)
        if not match:
            raise RuntimeError("MAC-Adresse konnte im esptool-Output nicht gefunden werden.")
        
        mac = match.group(1).upper()
        print(f"[+] MAC-Adresse gefunden: {mac} (Dauer: {t1-t0:.2f}s)")
        return mac
    except subprocess.CalledProcessError as e:
        print(f"[-] Fehler beim Ausführen von esptool.py. Ist der Port korrekt?\n{e.stderr}")
        sys.exit(1)
    except FileNotFoundError:
        print("[-] esptool.py wurde nicht im PATH gefunden. Bitte ESP-IDF Export-Skript ausführen (. export.sh).")
        sys.exit(1)

def derive_matter_passcode(mac: str) -> int:
    """
    Generiert deterministisch einen gültigen 8-stelligen Matter-Passcode aus der MAC.
    Matter Passcodes müssen im Bereich 1 bis 99999998 liegen und dürfen nicht in der RESTRICTED_PASSCODES Liste sein.
    """
    mac_bytes = bytes.fromhex(mac.replace(':', ''))
    
    # Nutze SHA-256 für eine deterministische, aber pseudo-zufällige Gleichverteilung
    digest = hashlib.sha256(mac_bytes).digest()
    num = int.from_bytes(digest[:4], byteorder='big')
    
    # Begrenze auf den Bereich 1 bis 99999998
    passcode = (num % 99999998) + 1
    
    # Stelle sicher, dass der Code nicht auf der Blacklist steht
    while passcode in RESTRICTED_PASSCODES:
        passcode = (passcode % 99999998) + 1
        
    print(f"[+] Generierter Matter Passcode aus MAC: {passcode:08d}")
    return passcode

def explain_spake2p(passcode: int):
    """Erklärt den SPAKE2+ Prozess und generiert einen CLI-Befehl."""
    salt = base64.b64encode(os.urandom(16)).decode('utf-8')
    iterations = 1000
    
    print("\n" + "="*60)
    print(" SPAKE2+ VERIFIER COMPUTATION (EXPERT INFO) ")
    print("="*60)
    print("Warum wird SPAKE2+ nicht in REINEM Python berechnet?")
    print("Matter nutzt für SPAKE2+ spezifische Elliptische Kurven (P-256) mit vordefinierten")
    print("M- und N-Punkten, die in Standard-Python-Kryptobibliotheken nicht out-of-the-box")
    print("implementiert sind. Wir müssen das offizielle C++ Tool aus dem SDK nutzen.\n")
    
    print("Performance-Aspekt:")
    print("Die SPAKE2+ (PBKDF2) Berechnung auf dem kleinen ESP32 dauert mehrere Sekunden")
    print("(zu langsam und blockierend für den Bootvorgang!). Auf dem PC dauert es Millisekunden.")
    print("Daher MUSS der Verifier auf dem Host berechnet und als NVS-Partition geflasht werden.\n")
    
    print("So kompilierst du das spake2p Tool auf dem PC (falls nicht vorhanden):")
    print("  cd components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/tools/spake2p")
    print("  gn gen out/host && ninja -C out/host\n")
    
    print("Sobald kompiliert, führt man diesen Befehl aus, um den Verifier zu berechnen:")
    cmd = f"./spake2p gen-verifier -p {passcode} -s {salt} -i {iterations}"
    print(f"--> {cmd}")
    print("\nDanach nimmt man den Output (Verifier-String) und nutzt das 'mfg_tool.py',")
    print("um die 'chip-factory' NVS Binärdatei zu bauen und diese auf den ESP32 zu flashen.")
    print("="*60 + "\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Matter MAC & Passcode Provisioning Helper")
    parser.add_argument("-p", "--port", required=True, help="Der serielle Port des ESP (z.B. /dev/ttyUSB0)")
    args = parser.parse_args()

    mac_address = get_esp_mac(args.port)
    passcode = derive_matter_passcode(mac_address)
    explain_spake2p(passcode)