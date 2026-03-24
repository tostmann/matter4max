#!/usr/bin/env python3
import serial
import time
import re
import sys
import qrcode
import subprocess

PORT = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00'
BAUD = 115200

def reset_esp(ser):
    print("Triggere Hardware-Reset via RTS/DTR (EN=LOW)...")
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setDTR(False)
    ser.setRTS(False)
    time.sleep(1.0) # USB-JTAG Enum delay

def get_matter_codes():
    print(f"Verbinde mit {PORT} ...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except Exception as e:
        print(f"Fehler beim Öffnen des Ports: {e}")
        sys.exit(1)

    reset_esp(ser)

    print("Warte auf Matter Pairing Codes im Boot-Log (Timeout in 15 Sekunden)...\n")
    end_time = time.time() + 15
    
    qr_code = None
    manual_code = None
    
    ser.reset_input_buffer()

    while time.time() < end_time:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
        except:
            continue
            
        if line:
            if "SetupQRCode" in line:
                qr_code = line
            if "Manual pairing code" in line:
                manual_code = line
        
        if qr_code and manual_code:
            break

    ser.close()

    print("\n" + "="*70)
    
    # Fallback to defaults since NVS was erased and standard Test-DAC is used.
    # The SDK disables console prints for these if not forced.
    if not manual_code:
        manual_code = "[34970112332]"
        qr_code = "[MT:Y.K9042C00KA0648G00]"
        print(" VERWENDE ESP-MATTER STANDARD-CODES (NVS Factory Defaults):\n")
    else:
        print(" MATTER PAIRING CODES IM LOG GEFUNDEN:\n")
        
    if manual_code:
        match_man = re.search(r'\[(\d+)\]', manual_code)
        if match_man:
            print(f" -> Manueller Pairing-Code (Home Assistant):   {match_man.group(1)}")
        else:
            print(f" -> Manueller Pairing-Code (Home Assistant):   {manual_code}")
            
    if qr_code:
        match_qr = re.search(r'\[(MT:.*?)\]', qr_code)
        if match_qr:
            code = match_qr.group(1)
            print(f" -> QR-Code Payload (String):                 {code}\n")
            
            qr = qrcode.QRCode()
            qr.add_data(code)
            qr.make(fit=True)
            print(" -> Terminal QR Code (Scanbar mit der Home Assistant App):")
            qr.print_ascii()
            
    print("="*70 + "\n")

if __name__ == '__main__':
    get_matter_codes()
