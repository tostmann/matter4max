import serial, time
ser = serial.Serial('/dev/ttyACM2', 115200, timeout=1)
ser.setDTR(False)
ser.setRTS(False)
time.sleep(0.1)
ser.setDTR(True)
ser.setRTS(True)
start = time.time()
while time.time() - start < 15:
    line = ser.readline()
    if line:
        print(line.decode('utf-8', 'replace').strip())
