# Bluetooth setup page

`index.html` is a single static page (no build, no dependencies) that configures the notifier over Bluetooth LE
with Web Bluetooth: Wi-Fi, time zone, hostname, and it shows the IP address once the board is online.

## Hosting
Any static HTTPS host works (GitHub Pages, your own server): Web Bluetooth refuses non-HTTPS pages, `localhost` excepted.
For a quick test: `python3 -m http.server -d setup 8000` and open `http://localhost:8000` in Chrome on the same machine.

## Browsers
Chrome and Edge on Android, Windows, macOS, Linux and ChromeOS. **Not iOS Safari or Chrome** (no Web Bluetooth there):
on an iPhone use the Bluefy browser. Firefox has no Web Bluetooth.

## Pairing and safety
The firmware advertises while no phone is connected. Every characteristic needs an authenticated, encrypted link
(LE Secure Connections, passkey): on the first connection the board draws a fresh 6-digit code over whatever page
is showing and the phone asks for it. The bond is kept in NVS (3 phones), later connections need no code. After 5
failed pairings the board stops advertising for 5 minutes. The code is never written to the log.

## GATT layout
Service `6e2b0001-3b7c-4f1e-9a52-5d0a6c1e7b30`; all characteristics are authenticated.

| UUID suffix | Properties | Content |
|---|---|---|
| `6e2b0002-…` status | read, notify | JSON `{"v","wifi":"none\|connecting\|connected","ssid","ip","host","tz","result","time","rssi"}`. A notification only means "changed": read it again (it can be longer than the MTU). `result` is the outcome of the last write: `ready`, `ok`, `restarting`, or the reason (`bad ssid`, `bad password`, `bad timezone`, `bad hostname`, `bad json`, `nothing to set`, `wifi failed`). |
| `6e2b0003-…` config | write | JSON, every key optional: `{"ssid","pass","tz","hostname","reboot"}`. `pass` needs `ssid`; empty `pass` = open network; `tz` is a POSIX TZ string; a bad value fails the write with ATT error 0x13 and the reason shows in `result`. Up to 512 bytes (long writes work). Wi-Fi and `tz` apply at once, `hostname` after a restart. |
| `6e2b0004-…` scan | read, write, notify | Write any byte to start a Wi-Fi scan (about 2 s); a notification follows, then read `[{"ssid","rssi","sec"}]`, strongest first, at most 480 bytes. |

Code: `firmware/components/ble/` (`core/provision.*` is host-tested, `idf/ble.cpp` is the NimBLE glue). The passkey overlay is
`screen/core/pair_view.*`.
