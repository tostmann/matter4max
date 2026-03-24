import serial
import time
import sys

try:
    s = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    
    print("Capturing ESP32 boot log (10s)...")
    t_end = time.time() + 10
    while time.time() < t_end:
        line = s.readline()
        if line:
            print(line.decode('utf-8', errors='ignore').strip())
except Exception as e:
    print("Error:", e)
