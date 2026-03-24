import serial
import time
import sys

try:
    s = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
    # Restart ESP32
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    s.close()
    time.sleep(1)
    s = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
    
    t_end = time.time() + 10
    while time.time() < t_end:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line and ('esp_matter_core' in line or 'Setup' in line or 'Passcode' in line or 'Discriminator' in line or 'QR' in line or 'NVS' in line):
            print(line)
except Exception as e:
    print("Error:", e)
