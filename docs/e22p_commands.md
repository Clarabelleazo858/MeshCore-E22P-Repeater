# E22P repeater commands

Commands are available through an authenticated MeshCore repeater console or
the local serial console at 115200 baud.

## OTA, Wi-Fi and web

| Command | Description |
| --- | --- |
| `start ota` | Start temporary `MeshCore-OTA` AP and OTA page for 10 minutes. |
| `stop ota` / `ota stop` | Stop temporary OTA; resume saved home STA if enabled. |
| `ota status` / `get ota` | Show STA, OTA, web and IP status. |
| `wifi on` | Persistently enable home STA; start WiFiManager if credentials are missing. |
| `wifi off` | Persistently disable STA, NTP and the Wi-Fi radio. |
| `wifi status` / `get wifi` | Show current Wi-Fi state. |
| `set web.password <password>` | Save a private 8-63 character web password. It is never displayed. |
| `web on` | Enable LAN web, HTTP CLI and LAN OTA. Requires a password. |
| `web off` | Disable LAN web while leaving STA available for NTP. |
| `web status` / `get web` | Show whether web is enabled and a password exists. |

The web username is always `admin`. On a new device, Wi-Fi and web access are
off. If no web password exists, `start ota` retains the original MeshCore 1.16
temporary AP behavior; diagnostic and command endpoints remain unavailable.
An older installation with STA already enabled temporarily reuses its MeshCore
admin password so LAN OTA remains reachable after upgrading.

## Radio and noise floor

| Command | Description |
| --- | --- |
| `get radio` | Show persisted frequency, bandwidth, spreading factor and coding rate. |
| `set radio <freq> <bw> <sf> <cr>` | Save radio parameters. Incorrect values can isolate the repeater. |
| `tempradio <freq> <bw> <sf> <cr> <minutes>` | Apply temporary radio parameters. |
| `get tx` | Show SX1262 TX power. |
| `set tx <dbm>` | Set TX power from -9 to 22 dBm. |
| `tx diag` / `get tx.diag` | Show stored, requested and applied TX power and RadioLib status. |
| `get agc.reset.interval` | Show the E22P AGC recovery interval. Default: 4 seconds. |
| `set agc.reset.interval <seconds>` | Set the interval; `0` disables periodic AGC recovery. |

## Diagnostics

| Command | Description |
| --- | --- |
| `health` | Health, RX errors, duplicates, queue, SNR, RSSI, noise floor and airtime. |
| `diag` | Uptime, reset reason, heap, queue, errors and OTA/Wi-Fi state. |
| `queue` | Packet queue, drop and allocation counters. |
| `dense` | Flood, advert, forwarding, drop and duplicate counters. |
| `cli stats` | Remote CLI receive, rejection and reply-path counters. |
| `clear dense` | Reset dense-mesh counters. |
| `clear stats` | Reset radio, packet and dense counters. |
| `time watchdog` | Show optional Wi-Fi NTP watchdog status. |
| `time sync` | Start an immediate NTP attempt when STA is connected. |

Use `help` or `help 1` through `help 6` for compact on-device command pages.
