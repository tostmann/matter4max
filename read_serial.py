import serial, time, sys
try:
    ser = serial.Serial('/dev/ttyACM2', 115200, timeout=1)
except Exception as e:
    print(e)
    sys.exit(1)
ser.setDTR(False)
ser.setRTS(False)
time.sleep(0.1)
ser.setDTR(True)
ser.setRTS(True)
start = time.time()
while time.time() - start < 15:
    line = ser.readline()
    if line:
        msg = line.decode('utf-8', 'replace').strip()
        print(msg)
