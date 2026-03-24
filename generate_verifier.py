import os
import hashlib
import base64
import sys

try:
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.backends import default_backend
except ImportError:
    print("Fehler: python 'cryptography' package fehlt. Bitte via 'pip install cryptography' installieren.")
    sys.exit(1)

def generate_matter_spake2p_verifier(passcode: int, iterations: int = 1000):
    # 1. Parameter definieren
    passcode_bytes = str(passcode).encode('utf-8')
    salt_bytes = os.urandom(16) # Matter erfordert 16 bis 32 Bytes Salt
    
    # Kurvenordnung q für SECP256R1 (P-256) nach RFC 9382
    q = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551

    # 2. PBKDF2 ausführen (Matter spezifiziert exakt 80 Bytes Output)
    ws = hashlib.pbkdf2_hmac('sha256', passcode_bytes, salt_bytes, iterations, 80)

    # 3. Splitten in w0s und w1s (jeweils 40 Bytes)
    w0s = ws[:40]
    w1s = ws[40:]

    # 4. Als Little-Endian Integer interpretieren und Modulo q rechnen
    w0 = int.from_bytes(w0s, byteorder='little') % q
    w1 = int.from_bytes(w1s, byteorder='little') % q

    # 5. w0 als 32-Byte Little-Endian serialisieren (Teil 1 des Verifiers)
    w0_bytes = w0.to_bytes(32, byteorder='little')

    # 6. L = w1 * P berechnen (Trick: w1 als Private Key laden)
    priv_key = ec.derive_private_key(w1, ec.SECP256R1(), default_backend())
    
    # Public Key unkomprimiert exportieren (0x04 || X || Y -> 65 Bytes)
    L_bytes = priv_key.public_key().public_bytes(
        encoding=serialization.Encoding.X962,
        format=serialization.PublicFormat.UncompressedPoint
    )

    # 7. Verifier zusammenbauen (32 Bytes w0 + 65 Bytes L = 97 Bytes)
    verifier_bytes = w0_bytes + L_bytes

    # 8. Ergebnisse Base64-kodieren für die ESP-IDF Konfiguration
    salt_b64 = base64.b64encode(salt_bytes).decode('utf-8')
    verifier_b64 = base64.b64encode(verifier_bytes).decode('utf-8')

    print("=== Matter SPAKE2+ Configuration ===")
    print(f"Passcode:   {passcode}")
    print(f"Iterations: {iterations}")
    print(f"Salt (B64): {salt_b64}")
    print(f"Verifier:   {verifier_b64}")
    print("====================================")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        PASSCODE = int(sys.argv[1])
    else:
        PASSCODE = 29644537
    ITERATIONS = 1000 
    
    generate_matter_spake2p_verifier(PASSCODE, ITERATIONS)