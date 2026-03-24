import serial
import sys
import time

try:
    s = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00', 115200, timeout=1)
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.dtr = True
    s.rts = False
    
    end_time = time.time() + 20
    while time.time() < end_time:
        line = s.readline()
        if line:
            sys.stdout.buffer.write(line)
            sys.stdout.flush()
except Exception as e:
    print(e)
