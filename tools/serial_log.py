"""Print serial output for N seconds: serial_log.py PORT [seconds] [--reset]"""
import sys, time, serial

port = sys.argv[1]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 8
with serial.Serial(port, 115200, timeout=0.2) as s:
    if "--reset" in sys.argv:
        s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
    end = time.time() + secs
    while time.time() < end:
        d = s.read(4096)
        if d:
            sys.stdout.write(d.decode(errors="replace")); sys.stdout.flush()
