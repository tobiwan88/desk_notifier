"""Send console commands to the device: console.py PORT "wifi ssid pass" "status" ..."""
import sys, time, serial

port, cmds = sys.argv[1], sys.argv[2:]
with serial.Serial(port, 115200, timeout=0.3) as s:
    s.read(65536)
    for c in cmds:
        s.write((c + "\r\n").encode())
        end = time.time() + 2.0
        buf = b""
        while time.time() < end:
            d = s.read(4096)
            if d:
                buf += d
                end = time.time() + 0.6
        print(buf.decode(errors="replace").strip())
