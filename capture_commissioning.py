import serial
import time
import sys

try:
    s = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
    print("Listening for 60s...")
    t_end = time.time() + 60
    while time.time() < t_end:
        line = s.readline()
        if line:
            print(line.decode('utf-8', errors='ignore').strip())
except Exception as e:
    print(e)
