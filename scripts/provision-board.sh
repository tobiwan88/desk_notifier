#!/usr/bin/env bash
# Flash one board with a variant and give it the Wi-Fi from .env (git-ignored).
# Usage: scripts/provision-board.sh <squirrel|bunny|stag|cat|penguin> <serial port>
# Run once per board; boards are programmed one after another.
set -eo pipefail

if [ $# -ne 2 ]; then
  echo "usage: $0 <squirrel|bunny|stag|cat|penguin> <serial port>" >&2
  exit 2
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
env_file="$root/.env"
if [ ! -f "$env_file" ]; then
  echo "no $env_file: create it with WIFI_SSID and WIFI_PASSWORD" >&2
  exit 1
fi

"$root/scripts/flash-variant.sh" "$1" "$2"

# Let the board boot its console before sending the Wi-Fi command.
sleep 8
uv run --project "$root" --with pyserial python - "$2" "$env_file" <<'PY'
import re, sys, time
import serial

port, env_path = sys.argv[1], sys.argv[2]
vals = {}
for line in open(env_path):
    m = re.match(r'^(WIFI_SSID|WIFI_PASSWORD)="(.*)"\s*$', line)
    if m:
        vals[m.group(1)] = m.group(2)
ssid, pw = vals["WIFI_SSID"], vals["WIFI_PASSWORD"]

s = serial.Serial(port, 115200, timeout=1)
s.dtr = False
s.rts = False
time.sleep(0.5)
s.read(s.in_waiting or 1)
s.write(f'wifi "{ssid}" "{pw}"\r\n'.encode())
end = time.time() + 3
buf = b""
while time.time() < end:
    buf += s.read(4096)
s.close()
ok = "saved; rebooting" in buf.decode(errors="replace")
print("wifi saved, board rebooting" if ok else "no confirmation from the board; check the serial log")
sys.exit(0 if ok else 1)
PY
