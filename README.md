# P1 — ESP-NOW Sensor Mesh + Cloud Bridge

Two ESP32 DevKit boards. Board 1 measures temperature and humidity and sends the reading
wirelessly. Board 2 measures its own temperature and humidity, receives Board 1's reading,
combines the two, and publishes a single JSON payload to ThingsBoard over MQTT.

There is no wire between the two boards. ESP-NOW is the entire link.

---

## 1. What this is, described honestly

This is a **two-node ESP-NOW prototype with a mesh-ready packet design.** It is not a
demonstrated multi-hop mesh, and the report should not claim to be one.

With two radios the physical topology can only be one hop: a root and a node. The packet
carries `ttl` and `hop_count` fields, and the addressing is source/destination based rather
than broadcast, so the format would not have to change if a third board were added and
forwarding logic written. But no packet in this project is ever forwarded, because there is
nowhere to forward it to. Claiming a working mesh from a one-hop link is the single easiest
way to lose marks on an otherwise solid project.

What **is** genuinely demonstrated:

| Concept | How it shows up |
|---|---|
| Peer-to-peer wireless without an access point | Board 1 never associates with the router, yet its data reaches the cloud |
| Root / gateway addressing | `src_id` / `dst_id` in every packet; Board 1 unicasts to Board 2's MAC |
| Edge aggregation | Board 2 merges two sensor sources into one payload before it leaves the network |
| Cloud fan-in | Two physical sensors, one MQTT device, one telemetry stream |
| Duplicate packet detection | `(src_id, boot_id, seq)` dedup with a `duplicate_count` counter |
| Local vs remote readings | `gateway_*` and `node1_*` keys published side by side |
| Liveness detection | `node1_online` flips to false 20 s after the last valid packet |

---

## 2. Hardware

- 2 × generic ESP32 DevKit, USB-C
- 2 × blue three-pin DHT11 temperature/humidity modules
- 2 × breadboards
- Jumper wires, USB data cables

No Raspberry Pi. No external LEDs or resistors. No wire between the boards.

### Board 1 — Remote Sensor Node

| Wire | ESP32 pin | DHT11 pin |
|---|---|---|
| Green | `3V3` | `+` / `VCC` |
| Orange | `GND` | `-` / `GND` |
| Yellow | `D4` = **GPIO4** | `S` / `OUT` / `DATA` |

### Board 2 — Root Gateway + Local Sensor

| Wire | ESP32 pin | DHT11 pin |
|---|---|---|
| Green | `3V3` | `+` / `VCC` |
| Orange | `GND` | `-` / `GND` |
| Yellow | `D5` = **GPIO5** | `S` / `OUT` / `DATA` |

The silkscreen label `D4` means GPIO4. The Arduino API takes the integer `4`, never the
string `"D4"`. Same for `D5` and `5`.

### Safety rules

- Both boards straddle the breadboard centre gap.
- Nothing connects to `VIN` or `EN`.
- Never join the two ESP32 boards with jumper wires. ESP-NOW is the link.
- Each board is powered separately over USB.
- **Connect only one board while uploading.** Two attached boards make it trivially easy to
  flash the gateway firmware onto the sensor node.
- Both boards are powered at the same time for the final demonstration.
- Sensors run at **3.3 V**. The three-pin DHT11 modules include their own data-line pull-up,
  so no external resistor is needed.

---

## 3. Why there are two sensors

Board 1 measures its own local environment and transmits it. Board 2 measures its own local
environment, receives Board 1's, and publishes both.

Without Board 2's sensor the project would only prove "a radio link works". With it, Board 2
is doing what a real gateway does: contributing its own data, merging it with data from the
field, and presenting one coherent view upstream. That merge step is the aggregation the
project is actually about.

---

## 4. Software substitution: DHT11 instead of BMP280

The original brief specified BMP280 pressure/temperature sensors. **The available hardware is
DHT11.** This is a deliberate, documented substitution.

Consequences, stated plainly:

- Measured quantities are **temperature (°C) and relative humidity (%)**.
- **No pressure is measured anywhere in this project.** No altitude either.
- No BMP280 libraries are installed and no I²C bus is used. The DHT11 is a single-wire
  bit-banged protocol on one GPIO, not an I²C device.
- The DHT11 is a low-resolution part: roughly ±2 °C and ±5 % RH, integer-ish steps. It is
  adequate for demonstrating a data pipeline and inadequate for precision claims.
- A DHT11 must not be read faster than once per second. This project reads every 2 s.

Everything else in the design — ESP-NOW, addressing, dedup, aggregation, MQTT, the dashboard
— is unaffected by the sensor choice.

---

## 5. Architecture

```
   BOARD 1  (Remote Sensor Node, node_id = 1)
   ┌──────────────────────────────┐
   │ DHT11 on GPIO4               │
   │ Wi-Fi station mode, radio up │
   │ NOT associated with any AP   │
   │ channel pinned by hand       │
   └──────────────┬───────────────┘
                  │  ESP-NOW unicast, 26-byte packet, every 5 s
                  │  addressed to Board 2's station MAC
                  ▼
   BOARD 2  (Root Gateway + Local Sensor, root_id = 0)
   ┌──────────────────────────────┐
   │ DHT11 on GPIO5               │
   │ associated with 2.4 GHz AP   │  <-- the AP decides the channel
   │ ESP-NOW receiver on that ch. │
   │ validate -> dedup -> merge   │
   └──────────────┬───────────────┘
                  │  MQTT, port 1883, every 10 s
                  │  topic v1/devices/me/telemetry
                  ▼
             ThingsBoard
```

### The single most important constraint

An ESP32 has **one radio**, so it is on **one channel** at a time. ESP-NOW frames are only
heard by a receiver sitting on the same channel.

Board 2 must join the access point to reach the cloud, and **the access point dictates Board
2's channel.** Board 2 cannot choose. So the channel is a value Board 2 *discovers and
reports*, and Board 1 is pinned to it by hand.

That is the whole reason for the upload order below: Board 2 goes first because it is the one
that finds out what the channel is.

### Packet format — 26 bytes

| Field | Type | Purpose |
|---|---|---|
| `version` | `uint8` | protocol version, `1`. Rejected if it does not match. |
| `msg_type` | `uint8` | `1` = sensor data. Room for future message types. |
| `src_id` | `uint8` | `1` = Board 1 |
| `dst_id` | `uint8` | `0` = root/gateway |
| `boot_id` | `uint32` | random per power-up. Distinguishes a restart from a replay. |
| `seq` | `uint32` | monotonic within one boot |
| `ttl` | `uint8` | `3`. Carried, never decremented — nothing to forward to. |
| `hop_count` | `uint8` | `0`. Same: mesh-ready, not mesh-exercised. |
| `temperature_c` | `float` | °C |
| `humidity_pct` | `float` | % RH |
| `uptime_ms` | `uint32` | sender's `millis()`, so the receiver can see node liveness |

26 bytes against an ESP-NOW limit of 250. `#pragma pack(1)` removes padding, and a
`static_assert(sizeof(mesh_packet_t) == 26)` in **both** sketches guarantees the two copies
of the struct agree. There is no shared-header mechanism between Arduino sketch folders, so
the struct is duplicated; the assertion is what stops that duplication from silently rotting
into "the gateway rejects every packet on length".

No checksum field. 802.11 already CRC-checks every frame it delivers, and the gateway's
length + version + range checks catch anything structurally wrong.

### Gateway validation

Every arriving frame must pass all of the following or it is discarded and counted as
invalid:

- length **exactly** `sizeof(mesh_packet_t)` — not "at least"
- `version == 1`
- `msg_type == 1`
- `dst_id == 0`
- `src_id == 1`
- neither float is `NaN`
- `-40 ≤ temperature_c ≤ 80`
- `0 ≤ humidity_pct ≤ 100`

The bounds are deliberately wider than the DHT11's own rated range so a cold room is not
mistaken for corruption. They are tuning knobs at the top of sketch 05.

### Duplicate detection

Key: `(src_id, boot_id, seq)`. Strategy: **strict last-sequence-wins** — within one sender
session, any sequence number at or below the highest already accepted is a duplicate.

That works because ESP-NOW unicast is acknowledged at the link layer and does not reorder over
a two-radio link, so a low sequence number means a retransmission rather than a late arrival. A
sliding-window scheme would tolerate reordering, but nothing here reorders. A sequence number
that jumps *forward* is treated as a gap, not a duplicate: those packets were lost, not
repeated, so the reading is accepted and the baseline moves up.

`boot_id` is the load-bearing part. When Board 1 restarts, its sequence numbers go back to 1.
Without `boot_id`, that looks identical to a glitch replaying old packets, and every
post-restart packet would be rejected forever — the node would simply never come back. With
it, a changed `boot_id` tells the gateway "new session, rebase".

### Callback discipline

The ESP-NOW receive callback runs on the **Wi-Fi task**, not the Arduino task. It validates,
copies into a FreeRTOS queue, and returns. It does no MQTT, no sensor reads, no delays, and
no Serial printing. Blocking that callback starves the Wi-Fi driver, drops the frames queued
behind the one being handled, and can trip the task watchdog.

Dedup, bookkeeping, logging, and publishing all happen in `loop()`, where blocking is
harmless. A queue is used rather than a shared struct plus a flag because it gives correct
cross-task handoff without hand-rolled locking.

All scheduling in sketches 04 and 05 uses `millis()` comparisons. No blocking `delay()` in
either main loop.

---

## 6. Environment

Arduino IDE projects. Not PlatformIO.

| Setting | Value |
|---|---|
| Board | **ESP32 Dev Module** |
| FQBN (arduino-cli) | `esp32:esp32:esp32` |
| Core | Arduino-ESP32 **3.3.3** (stable 3.x, built on ESP-IDF 5.5) |
| Serial Monitor | **115200** |

### Libraries

| Library | Author | Version used |
|---|---|---|
| DHT sensor library | Adafruit | 1.4.7 |
| Adafruit Unified Sensor | Adafruit | 1.1.15 |
| PubSubClient | Nick O'Leary | 2.8 |

Built-in headers used: `WiFi.h`, `esp_now.h`, `esp_wifi.h`.

Install via **Tools → Manage Libraries…** in the IDE, or on the command line:

```
arduino-cli lib install "DHT sensor library" "Adafruit Unified Sensor" "PubSubClient"
```

Note the near-miss names in the library index. You want `PubSubClient`, not `PubSubClient3`,
`TBPubSubClient`, or `MQTTPubSubClient`. You want `DHT sensor library`, not `DHT11`,
`DHTlib`, or `DHT sensor library for ESPx`.

### ESP-NOW callback signatures on this core

Arduino-ESP32 3.3.x sits on ESP-IDF 5.5, which **changed both ESP-NOW callback signatures.**
Nearly every ESP-NOW tutorial online predates this and will not compile:

```c
// This core (verified in the installed esp_now.h):
void onSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);   // MAC: tx_info->des_addr
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int data_len); // MAC: info->src_addr

// The old form, which will NOT compile here:
void onSent(const uint8_t *mac, esp_now_send_status_t status);
void onRecv(const uint8_t *mac, const uint8_t *data, int data_len);
```

If you copy code in from elsewhere and get a cryptic error about an incompatible pointer or
argument type on `esp_now_register_send_cb`, this is why.

### build.ps1

A thin wrapper around the arduino-cli bundled inside Arduino IDE 2 (it is not on `PATH`).
Optional — the IDE does the same jobs, except for `-NoReset`.

`-NoReset` matters for the stale-detection and reboot tests. Opening a serial port normally
pulses RTS, which resets the ESP32 — and a reset wipes exactly the accumulated state those
tests are about: `node1_age_ms`, the sequence baseline, the duplicate counter. The Arduino IDE
Serial Monitor resets on connect too, so use `-NoReset` when you need to observe a board that
is already running without disturbing it.

```powershell
.\build.ps1 -Ports                                 # list boards and COM ports
.\build.ps1 -All                                   # compile all five sketches
.\build.ps1 -Sketch 01                             # compile one
.\build.ps1 -Sketch 01 -Upload -Port COM3
.\build.ps1 -Monitor -Port COM3 -Seconds 20        # read serial, then exit
.\build.ps1 -Monitor -NoReset -Port COM4           # attach without resetting the board
.\build.ps1 -Sketch 01 -Upload -Monitor -Port COM3
.\build.ps1 -Sketch 01 -Upload -Port COM3 -UploadSpeed 115200   # slow, reliable fallback
```

---

## 7. Project layout

```
P1_espnow_mesh/
  01_board1_dht_test/         Board 1 sensor bring-up, GPIO4
  02_board2_dht_test/         Board 2 sensor bring-up, GPIO5
  03_board2_gateway_info/     Board 2 station MAC + 2.4 GHz scan
  04_board1_espnow_sender/    Board 1 final: ESP-NOW transmitter
  05_board2_gateway_cloud/    Board 2 final: receiver + aggregator + MQTT
    secrets.example.h         committed template
    secrets.h                 real credentials, GITIGNORED
  README.md
  build.ps1
  .gitignore
```

`secrets.h` lives inside the sketch folder because the Arduino build only sees files
alongside the `.ino`. Sketch 04 needs no secrets — it never joins the network — so its two
configurable values sit in a marked `CONFIG` block at the top of the `.ino`.

---

## 8. Exact upload order

Do not skip ahead. Each stage exists to make the next one debuggable, and stages 6 and 9
produce values that later stages cannot be built without.

| # | Board | Sketch | What it proves / produces |
|---|---|---|---|
| 1 | 1 | `01_board1_dht_test` | compiles and uploads |
| 2 | 1 | — | **valid Board 1 readings** |
| 3 | 2 | `02_board2_dht_test` | compiles and uploads |
| 4 | 2 | — | **valid Board 2 readings** |
| 5 | 2 | `03_board2_gateway_info` | — |
| 6 | 2 | — | **record Board 2's station MAC** |
| 7 | — | ThingsBoard + `secrets.h` | device created, token and Wi-Fi filled in |
| 8 | 2 | `05_board2_gateway_cloud` | gateway online first |
| 9 | 2 | — | **record the connected Wi-Fi channel** |
| 10 | 1 | edit `04_...` | paste in the MAC (step 6) and channel (step 9) |
| 11 | 1 | `04_board1_espnow_sender` | upload |
| 12 | both | — | power both boards |
| 13 | 2 | — | confirm ESP-NOW packets arrive |
| 14 | 2 | — | confirm duplicates are rejected |
| 15 | cloud | — | confirm combined JSON reaches ThingsBoard |
| 16 | cloud | — | build dashboard widgets |

Connect **one** board at a time for every upload.

---

## 9. Expected Serial Monitor output

All output is prefixed `[NODE]` for Board 1 and `[GATEWAY]` for Board 2. Numbers below are
illustrative.

### `01_board1_dht_test` — Board 1

```
[NODE] ============================================
[NODE] 01_board1_dht_test
[NODE] DHT11 on GPIO4, reading every 2000 ms
[NODE] ============================================
[NODE] Note: the first read or two often return NaN on a DHT11.
[NODE] That is normal. Persistent NaN means a wiring problem.
[NODE] read 1 FAILED (NaN)  fails=1
[NODE]   check: yellow data wire on GPIO4, green on 3V3, orange on GND
[NODE] read 2  temperature=23.0 C  humidity=45.0 %  (fails=1)
[NODE] read 3  temperature=23.0 C  humidity=45.0 %  (fails=1)
```

One or two leading NaN reads are normal and expected. What matters is that readings settle
into plausible values and `fails` stops climbing.

### `02_board2_dht_test` — Board 2

Identical, with `[GATEWAY]` and `GPIO5`.

### `03_board2_gateway_info` — Board 2

```
[GATEWAY] 03_board2_gateway_info
[GATEWAY] station MAC (human readable) : XX:XX:XX:XX:XX:XX
[GATEWAY]
[GATEWAY] Copy the next line into 04_board1_espnow_sender.ino:
[GATEWAY] static uint8_t GATEWAY_MAC[6] = { 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX };
[GATEWAY]
[GATEWAY] scanning for 2.4 GHz access points ...
[GATEWAY] 7 network(s) visible:
[GATEWAY]   ch  rssi  security  ssid
[GATEWAY]    6   -48  secured   YourNetwork
[GATEWAY]   11   -71  secured   Neighbour
[GATEWAY] Every channel above is a 2.4 GHz channel, because that is
[GATEWAY] the only band this radio has. A missing SSID means 5 GHz only.
```

**This sketch prints a paste-ready C array line.** Use it. Hand-transcribing six hex bytes is
where this step usually goes wrong.

If your home network does not appear in the scan list, it is 5 GHz-only and Board 2 will
never connect to it.

### `05_board2_gateway_cloud` — Board 2

```
[GATEWAY] 05_board2_gateway_cloud
[GATEWAY] packet size = 26 bytes (ESP-NOW limit is 250)
[GATEWAY] connecting to "YourNetwork" ...
....
[GATEWAY] Wi-Fi connected
[GATEWAY] station MAC = XX:XX:XX:XX:XX:XX
[GATEWAY] ip=192.168.1.42  rssi=-49 dBm
[GATEWAY] --------------------------------------------------------
[GATEWAY] ACTIVE 2.4 GHz CHANNEL = 6
[GATEWAY] Put this in sketch 04: ESPNOW_CHANNEL = 6
[GATEWAY] --------------------------------------------------------
[GATEWAY] ESP-NOW listening on channel 6, expecting 26 byte packets
[GATEWAY] MQTT target thingsboard.cloud:1883 topic v1/devices/me/telemetry client_id p1gw-XXXXXXXXXXXX
[GATEWAY] publishing every 10000 ms, node goes stale after 20000 ms
[GATEWAY] MQTT connecting to thingsboard.cloud:1883 ...
[GATEWAY] MQTT connected
[GATEWAY] ESP-NOW rx from XX:XX:XX:XX:XX:XX  node=1 seq=1 boot_id=2914... temperature=22.0 C humidity=48.0 % node_uptime=5231 ms ttl=3 hops=0
[GATEWAY] local_ok=yes  node1_online=yes  node1_age=1204 ms  received=1  duplicates=0  invalid=0
[GATEWAY] MQTT published 268 bytes: {"gateway_temperature":23.0,"gateway_humidity":45.0,"gateway_sensor_ok":true,"node1_temperature":22.0,"node1_humidity":48.0,"node1_sequence":1,"node1_age_ms":1204,"node1_online":true,"duplicate_count":0,"espnow_received_count":1}
```

**Copy the `ACTIVE 2.4 GHz CHANNEL` number into sketch 04.** This is step 9 of the upload
order, and it is the value the whole link depends on.

### `04_board1_espnow_sender` — Board 1

```
[NODE] 04_board1_espnow_sender
[NODE] boot_id = 2914773641
[NODE] packet size = 26 bytes (ESP-NOW limit is 250)
[NODE] channel requested=6 actual=6
[NODE] own station MAC = YY:YY:YY:YY:YY:YY
[NODE] Wi-Fi association: none (by design), status=6
[NODE] ESP-NOW initialised
[NODE] gateway peer added: XX:XX:XX:XX:XX:XX on channel 6
[NODE] sending every 5000 ms
[NODE] tx seq=1 temperature=22.0 C humidity=48.0 % uptime=5231 ms
[NODE] send to XX:XX:XX:XX:XX:XX : OK (link-layer ack received)
```

Three lines are worth reading closely:

- `channel requested=6 actual=6` — proves the channel pin took. A mismatch here means the
  whole demo will fail silently.
- `Wi-Fi association: none (by design), status=6` — status 6 is `WL_DISCONNECTED`. This is
  the evidence that Board 1 is **not** on the access point.
- `OK (link-layer ack received)` — this is a real ESP-NOW acknowledgement from Board 2's MAC
  layer, not a hopeful assumption. `FAILED (no ack)` means nothing is listening: wrong MAC,
  wrong channel, or Board 2 is off.

---

## 10. ThingsBoard setup

1. Create an account at <https://thingsboard.cloud> (or use your self-hosted instance).
2. **Entities → Devices → +** → add a device, name it something like `P1 Gateway`.
3. Open the device → **Manage credentials** → credential type **Access token** → copy the
   token.
4. Copy `05_board2_gateway_cloud/secrets.example.h` to `secrets.h` in the same folder and
   fill in all four values.
5. Upload sketch 05 to Board 2.
6. Back in ThingsBoard, open the device → **Latest telemetry**. Keys should appear within
   about 10 seconds.

### MQTT contract

| Item | Value |
|---|---|
| Host | `thingsboard.cloud` (or your host) |
| Port | `1883` (plain MQTT) |
| Username | the device **access token** |
| Password | **empty** |
| Client ID | `p1gw-<MAC>`, generated by the sketch |
| Topic | `v1/devices/me/telemetry` |
| Payload | flat JSON object |

The access token going in the **username** field with an empty password is ThingsBoard's
access-token scheme. It surprises people who expect the token to be the password.

`PubSubClient`'s default buffer is 256 bytes and the payload is around 280. Sketch 05 calls
`mqtt.setBufferSize(512)`. Without it, `publish()` silently returns `false` and nothing ever
reaches the cloud — no error, no warning, just an empty dashboard.

### Dashboard telemetry keys

| Key | Type | Meaning |
|---|---|---|
| `gateway_temperature` | number | Board 2's local DHT11, °C |
| `gateway_humidity` | number | Board 2's local DHT11, % RH |
| `gateway_sensor_ok` | boolean | last local read succeeded |
| `node1_temperature` | number | Board 1's DHT11, °C, via ESP-NOW |
| `node1_humidity` | number | Board 1's DHT11, % RH, via ESP-NOW |
| `node1_sequence` | number | sequence number of the last accepted packet |
| `node1_age_ms` | number | ms since the last valid packet from Board 1 |
| `node1_online` | boolean | false once `node1_age_ms` exceeds 20000 |
| `duplicate_count` | number | packets rejected as duplicates since boot |
| `espnow_received_count` | number | packets that passed validation since boot |

Sparse-key behaviour, by design: a value key is **omitted** until there is a real number for
it. Before the first successful local read there is no `gateway_temperature`; before the
first packet from Board 1 there is no `node1_temperature`, `node1_humidity`,
`node1_sequence`, or `node1_age_ms`. The `gateway_sensor_ok` and `node1_online` flags always
ship, so the dashboard can always explain the gap.

This exists because `snprintf` renders `NaN` as `nan`, which is not valid JSON, and
ThingsBoard discards the **entire message** rather than the one bad field. Omitting a key is
strictly better than poisoning the payload. Once a valid reading has been seen, the last
known good value keeps being published and the `_ok` / `_online` flag carries the truth about
freshness — so widgets do not blank out when a sensor hiccups.

### Suggested widgets

| Widget | Keys |
|---|---|
| Time series line chart | `gateway_temperature`, `node1_temperature` |
| Time series line chart | `gateway_humidity`, `node1_humidity` |
| Digital gauge ×2 | `gateway_temperature`, `node1_temperature` |
| LED indicator | `node1_online` |
| LED indicator | `gateway_sensor_ok` |
| Card / label | `espnow_received_count`, `duplicate_count`, `node1_sequence` |

Two temperatures on one chart is the strongest single visual: it shows two physically
separate sensors, one of which has no network connection of its own, arriving as one stream.

---

## 11. Troubleshooting

### No COM port appears when a board is plugged in

The most common first-time failure, and not a code problem. Open Device Manager and look for
an unknown device or a yellow warning triangle under **Ports (COM & LPT)** or **Other
devices**. Generic DevKits use either a **CH340** or a **CP2102** USB-serial chip, and
Windows does not always have the driver.

Also confirm the USB cable is a **data** cable. Charge-only cables are common and produce
exactly this symptom.

Check with `.\build.ps1 -Ports`.

### Upload stalls on `Connecting........_____`

Hold the **BOOT** button while the upload is retrying, release once it starts writing. Some
DevKits have an unreliable auto-reset circuit.

If it connects but fails partway through, drop the baud rate:

```powershell
.\build.ps1 -Sketch 01 -Upload -Port COM3 -UploadSpeed 115200
```

### Upload fails with the port busy

A Serial Monitor still has the port open. Close it. Only one program can hold a COM port.

### DHT11 always reads NaN

- One or two NaN reads at startup are normal. **Persistent** NaN is a wiring fault.
- Check the yellow wire is on the right pin: GPIO4 on Board 1, GPIO5 on Board 2.
- Check green on `3V3` and orange on `GND`, and that the module is on 3.3 V not VIN.
- Confirm the board straddles the breadboard centre gap so the two pin rows are not shorted.
- Press the module's pins fully into the breadboard. A loose data line reads as NaN.
- Try the other DHT11 module to rule out a dead sensor.

### Board 2 will not connect to Wi-Fi

- **The network must be 2.4 GHz.** An ESP32 has no 5 GHz radio. Run sketch 03 — if the SSID
  is not in the scan list, the ESP32 cannot see it.
- Check SSID spelling and password in `secrets.h`, including case.
- Sketch 05 prints `status=` on failure. `6` is disconnected, `4` is connect-failed
  (usually a wrong password), `1` is no-SSID-available (usually 5 GHz or out of range).

### WPA2-Enterprise / eduroam

University networks such as eduroam do not use a shared password. Each user authenticates
with their own account against a RADIUS server using 802.1X. Set `WIFI_USE_ENTERPRISE` to `1`
in `secrets.h` and fill in the three EAP values; sketch 05 then calls the PEAP form of
`WiFi.begin()` instead of the pre-shared-key form.

This works because the installed core has `CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT=y`. It costs
about 97 KB of flash — the gateway sketch goes from roughly 69% to 77% full.

If it will not authenticate:

- **Identity vs username.** The identity is the *outer* identity, sent in the clear during the
  handshake. Some institutions require an anonymous outer identity such as
  `anonymous@example.ac.nz`; others want the same string as the username. If unsure, set both
  to your full `user@institution` login.
- **Domain suffix.** Many deployments reject a bare username and require the full
  `user@institution.ac.nz` form.
- **Server certificate validation.** This sketch passes `NULL` for the CA bundle, so it does
  not verify the RADIUS server. Networks configured to require client-side validation will
  refuse the connection, and there is no way around that without loading the institution's CA
  certificate into the sketch.

Two warnings worth taking seriously:

1. **The credentials are stored in flash as plain text.** Anyone with physical access to the
   board's USB port can recover them with one `esptool read-flash` command. On eduroam those
   are usually your full institutional account — the same login as your email. Treat a flashed
   board as a device that carries your password, and re-flash something harmless before
   lending it out.
2. **Campus firewalls commonly block outbound port 1883.** Authentication can succeed
   perfectly and MQTT still fail with `state=-2`. If that happens the network is the problem,
   not the sketch. A phone hotspot is the usual workaround and needs only
   `WIFI_USE_ENTERPRISE 0` plus an SSID and password.

### Board 1 says `send to ... : FAILED (no ack)`

In order of likelihood:

1. **`GATEWAY_MAC` is wrong or still the placeholder.** Sketch 04 prints a loud warning at
   boot if it is still `AA:AA:AA:AA:AA:AA`. Re-run sketch 03 and copy the paste-ready line.
2. **Channel mismatch.** Board 1's `channel requested=N actual=N` must equal the
   `ACTIVE 2.4 GHz CHANNEL` that sketch 05 printed. Re-check both numbers.
3. **You used Board 2's soft-AP MAC instead of its station MAC.** They differ by one bit.
   Sketch 03 prints the station MAC specifically.
4. Board 2 is powered off, or still running a different sketch.

### Sends report OK but the gateway logs nothing

Almost always the channel. `OK` means Board 2's MAC layer acknowledged the frame, so the two
radios *are* on the same channel and the MAC is right — but if the gateway prints no `rx`
line, check `invalid=` in the gateway's status line. A non-zero `invalid` count means frames
are arriving and failing validation, which points at a packet-struct mismatch between the two
sketches. Both should print `packet size = 26 bytes` at boot.

### Gateway received count stalls or misses packets

Confirm sketch 05 still calls `WiFi.setSleep(false)`. With power save on, a connected station
parks its radio between access point beacons and simply does not hear ESP-NOW frames during
those windows. The symptom is intermittent loss that looks like poor range.

### `node1_online` never goes true

Board 1 is not reaching Board 2 at all. Work through the `FAILED (no ack)` list above.

### MQTT connect fails

Sketch 05 prints `state=`:

| state | Meaning |
|---|---|
| `-4` | connection timeout |
| `-2` | TCP connect failed — wrong host, wrong port, or a firewall blocking 1883 |
| `4` | bad credentials — check the access token |
| `5` | not authorised — token not accepted for that device |

On a school or corporate network, outbound port 1883 is often blocked. Test from a phone
hotspot to isolate that.

### Telemetry publishes but nothing shows in ThingsBoard

- Confirm the token in `secrets.h` matches the device you are looking at.
- Check the serial log actually says `MQTT published`, not `MQTT publish FAILED` or
  `MQTT offline, payload not sent`.
- Look at **Latest telemetry** on the device page, not the dashboard — an empty dashboard
  with populated latest-telemetry means the widget is misconfigured, not the device.

### Wi-Fi reconnected but ESP-NOW stopped working

Sketch 05 detects this and prints `WARNING channel changed N -> M`. Routers with automatic
channel selection move channels. Board 1 is pinned to the old one. Put the new channel in
sketch 04 and re-upload it. This failure is nasty precisely because Wi-Fi and MQTT stay
perfectly healthy while the mesh half goes dead.

### `duplicate_count` never leaves zero

In normal operation this is **correct, not broken**. ESP-NOW unicast is acknowledged at the
link layer, so genuine duplicates are rare. A counter sitting at zero means nothing has been
duplicated, not that detection is dead.

To prove the mechanism works, force it: set `SEND_DUPLICATE_FOR_DEMO` to `1` in sketch 04 and
re-upload. Every reading is then transmitted twice with the same sequence number, so the
gateway accepts one and counts exactly one duplicate per reading. Set it back to `0`
afterwards.

If `duplicate_count` stays at zero even with that flag on, check that sketch 05 does not print
the `isDuplicate() is still the stub` warning at boot. That warning appears when
`DEDUP_IMPLEMENTED` is `0`.

---

## 12. Acceptance criteria

- [ ] Both DHT11 sensors produce valid readings (sketches 01 and 02)
- [ ] Board 1 is **not** connected to the Wi-Fi access point — `Wi-Fi association: none`
- [ ] Board 1 sends readings over ESP-NOW — `send to ... : OK`
- [ ] Board 2 receives them — `ESP-NOW rx from ...`, `received=` climbing
- [ ] Board 2 deduplicates — `DUPLICATE rejected`, `duplicates=` climbing
- [ ] Board 2 also reads its own sensor — `local_ok=yes`
- [ ] Both readings appear in one JSON payload
- [ ] ThingsBoard receives live telemetry
- [ ] Serial logs clearly show node, gateway, ESP-NOW, Wi-Fi and MQTT status
- [ ] Unplugging Board 1 flips `node1_online` to false after 20 s
- [ ] Restarting Board 1 produces a new `boot_id` and does not break deduplication

---

## 13. Demo evidence checklist

Capture these for the report. Each one maps to a specific claim.

| Evidence | Proves |
|---|---|
| Photo of both breadboards, wires visible | wiring matches the tables, no board-to-board wire |
| Board 1 serial: `01_...` readings | Board 1's sensor works |
| Board 2 serial: `02_...` readings | Board 2's sensor works |
| Board 2 serial: `03_...` station MAC | how the address was obtained, not guessed |
| Board 2 serial: `ACTIVE 2.4 GHz CHANNEL = N` | channel is discovered, not assumed |
| Board 1 serial: `channel requested=N actual=N` | the channel pin took effect |
| Board 1 serial: `Wi-Fi association: none (by design)` | Board 1 has no AP connection |
| Board 1 serial: `send to ... : OK` | link-layer ack, not a hopeful send |
| Board 2 serial: `ESP-NOW rx from ...` | packets arrive and pass validation |
| Board 2 serial: `DUPLICATE rejected` | dedup works (use `SEND_DUPLICATE_FOR_DEMO`) |
| Board 2 serial: full `MQTT published` JSON line | both sensors in one payload |
| ThingsBoard **Latest telemetry** screenshot | cloud ingest works |
| ThingsBoard dashboard: both temperatures, one chart | edge aggregation, visually |
| Board 2 serial with Board 1 unplugged 20 s+ | `node1_online:false`, stale detection |
| Board 2 serial after Board 1 restart | new `boot_id`, no duplicate flood |

For the report, describe the topology as a **two-node ESP-NOW prototype with a mesh-ready
packet design**. Say plainly that multi-hop forwarding is designed for but not demonstrated,
because two radios cannot demonstrate it. Naming that limit yourself is worth more than
hoping nobody asks.

---

## 14. Values you must fill in by hand

Nothing real is hard-coded anywhere in this project. Four values are yours:

| # | Where | Value | Comes from |
|---|---|---|---|
| 1 | `05_.../secrets.h` | `WIFI_SSID` | your 2.4 GHz network |
| 2 | `05_.../secrets.h` | `WIFI_USE_ENTERPRISE` | `0` for a hotspot or home router, `1` for eduroam |
| 3 | `05_.../secrets.h` | `WIFI_PASSWORD` *or* the three `WIFI_EAP_*` values | depending on the line above |
| 4 | `05_.../secrets.h` | `MQTT_HOST` | `thingsboard.cloud`, or your host |
| 5 | `05_.../secrets.h` | `THINGSBOARD_ACCESS_TOKEN` | ThingsBoard device → Manage credentials |
| 6 | `04_....ino` | `GATEWAY_MAC[6]` | printed by sketch 03 on Board 2 |
| 7 | `04_....ino` | `ESPNOW_CHANNEL` | printed by sketch 05 on Board 2 |

Both sketch-04 values are printed in paste-ready form by the sketches that discover them.
Use the printed lines rather than retyping hex.
