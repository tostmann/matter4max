import serial
import time
try:
    s = serial.Serial('/dev/serial/by-id/usb-busware.de_CUL868-if00', 38400, timeout=1)
    s.write(b'Zr\n')
    time.sleep(0.5)
    
    timeout_time = time.time() + 10
    print("Transmitting dummy MAX! packets...")
    while time.time() < timeout_time:
        s.write(b'Zs0D8C047001D00400000000580411\n')
        print("Sent Zs0D8C047001D00400000000580411")
        time.sleep(1)
except Exception as e:
    print("Error:", e)
