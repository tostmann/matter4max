import serial
import time
import sys

try:
    s = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    
    t_end = time.time() + 10
    while time.time() < t_end:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if "wifi" in line or "ip" in line or "Network" in line or "IPv4" in line or "IPv6" in line:
            print(line)
except Exception as e:
    pass
