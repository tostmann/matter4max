import serial
import time

try:
    s = serial.Serial('/dev/ttyACM1', 115200)
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    
    print("Capturing ESP32 boot log...")
    t_end = time.time() + 5
    while time.time() < t_end:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if "Manual pairing code" in line or "SetupQRCode" in line or "qrcode.html" in line:
            print("FOUND:", line)
except Exception as e:
    print("Error:", e)
