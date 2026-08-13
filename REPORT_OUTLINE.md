# P1 Report — structure and evidence map

A skeleton to write into. Each section lists what belongs there and which evidence file
backs it up. The analysis and discussion are yours to write; this maps out where the
material goes and what you already have to support it.

All evidence lives in `evidence/`. ThingsBoard screenshots are the four images captured
from the dashboard and device pages.

---

## 1. Introduction

- What the system does, in three or four sentences.
- Two ESP32 boards. Board 1 measures and transmits; Board 2 measures, receives, merges,
  and publishes to the cloud.
- State the scope honestly up front: a **two-node ESP-NOW prototype with a mesh-ready
  packet design**, not a demonstrated multi-hop mesh.

**Evidence:** none needed. Set expectations here so section 9 is not a surprise.

---

## 2. Hardware

- Component list: 2 × ESP32 DevKit, 2 × DHT11, 2 × breadboards, jumper wires, USB cables.
- Wiring tables for both boards (copy from `README.md` §2).
- Note the safety constraints observed: nothing on `VIN`/`EN`, 3.3 V only, boards straddle
  the breadboard centre gap, no wire between the two boards.

**Evidence:**
- `photo_02_wiring_detail_dht11.jpeg` — wire colours, DHT11 module, board straddling the gap
- `photo_03_board_on_breadboard.jpeg` — pin labels legible
- `photo_01_both_boards_no_wire.jpeg` — both boards, separately powered, nothing between them

Lead with `photo_01`. It is the only artefact that physically proves the link is wireless;
every other piece of evidence is a number on a screen.

---

## 3. Hardware substitution: DHT11 instead of BMP280

State this plainly rather than burying it.

- The brief specified BMP280 (pressure + temperature). The available hardware was DHT11.
- Measured quantities are therefore **temperature and relative humidity**. No pressure is
  measured anywhere in this project.
- Consequences: DHT11 is a single-wire bit-banged protocol, not I²C. Resolution is roughly
  ±2 °C and ±5 % RH, and the part must not be read faster than once per second.
- What is unaffected: ESP-NOW, addressing, deduplication, aggregation, MQTT, dashboard.

**Your analysis:** did the substitution weaken the demonstration? Argue it either way, but
argue it — the marker will be looking for whether you noticed it mattered.

---

## 4. Architecture

- Diagram: Board 1 → ESP-NOW → Board 2 → MQTT → ThingsBoard. Copy the ASCII diagram from
  `README.md` §5 or redraw it.
- Explain the roles: node (`src_id = 1`) and root/gateway (`dst_id = 0`).
- **The single-radio constraint.** This is the most important design fact in the project.
  An ESP32 has one radio and therefore one channel. Board 2 must join the access point for
  cloud access, and the access point dictates its channel. Board 2 therefore *discovers*
  its channel; Board 1 is pinned to that value by hand. This is why the upload order is
  Board 2 first.

**Evidence:**
- `02_gateway_boot_channel.txt` — `ACTIVE 2.4 GHz CHANNEL = 1`, discovered at runtime
- `03_node_boot_and_send.txt` — `channel requested=1 actual=1`, read back to confirm

**Your analysis:** what would break if Board 1 also associated with the access point?

---

## 5. Packet design

- The 26-byte struct table (copy from `README.md` §5).
- Why each field exists. `boot_id` deserves its own paragraph — see section 6.
- `ttl` and `hop_count` are carried but never decremented. Say so, and say why: there is
  nothing to forward to in a one-hop topology. They exist so the format would not have to
  change if the network grew.
- Why there is no checksum: 802.11 already CRC-checks every delivered frame, and the
  gateway's length, version and range checks catch anything structurally wrong.

**Validation performed by the gateway:** exact length, protocol version, message type,
destination ID, source ID, non-NaN floats, plausible temperature and humidity ranges.

**Evidence:** `01_gateway_running_state.txt` — `invalid=0` across 634 packets.

---

## 6. Deduplication

Key: `(src_id, boot_id, seq)`. Strategy: strict last-sequence-wins.

- Within one sender session, any sequence number at or below the highest already accepted
  is a duplicate.
- Justification: ESP-NOW unicast is link-layer acknowledged and does not reorder over a
  two-radio link, so a low sequence number means a retransmission, not a late arrival.
- A sequence number that jumps *forward* is a gap, not a duplicate — those packets were
  lost, not repeated — so it is accepted and the baseline moves up.
- **`boot_id` is the load-bearing part.** Board 1 randomises it every power-up and restarts
  sequencing at 1. Without it, a restart would look identical to a replay and the node
  would never come back.

**Evidence:**
- `01_gateway_running_state.txt` — `duplicates=82` out of 164 sent (forced with
  `SEND_DUPLICATE_FOR_DEMO`), exactly half rejected
- `03_node_boot_and_send.txt` + `04_gateway_accepts_new_bootid.txt` — new `boot_id`,
  sequence rebased, `duplicates=0`

**Worth mentioning:** the gateway was also restarted mid-stream during testing, wiping its
dedup memory while Board 1 kept transmitting at `seq=475`. It re-synced correctly. That is
the mirror case of the stated criterion and was not planned for.

---

## 7. Implementation notes

Three decisions worth defending in writing, because each one is a bug avoided rather than a
feature added:

1. **Callback discipline.** The ESP-NOW receive callback runs on the Wi-Fi task. It
   validates, copies into a FreeRTOS queue, and returns — no MQTT, no sensor reads, no
   delays, no printing. Blocking there starves the Wi-Fi driver and can trip the task
   watchdog. All processing happens in `loop()`.
2. **`WiFi.setSleep(false)`.** A connected station parks its radio between AP beacons by
   default, and a parked radio does not hear ESP-NOW frames. One line; the difference
   between reliable reception and unexplained intermittent loss.
3. **`mqtt.setBufferSize(512)`.** PubSubClient defaults to 256 bytes and silently returns
   `false` above that. The full MQTT packet here is ~259 bytes (23-byte topic + 231-byte
   payload + headers), and the payload *grows* once remote data arrives — so the default
   would have worked in early testing and failed exactly when the mesh started working.

Also worth a line: NaN never reaches the JSON. `snprintf` renders NaN as `nan`, which is
invalid JSON, and ThingsBoard discards the entire message rather than the one bad field.
Value keys are omitted until a real reading exists; the `_ok` / `_online` flags always ship.

---

## 8. Testing and results

Structure this as the acceptance criteria table. One row per criterion, with the evidence
file and the observed result.

| # | Criterion | Evidence | Result |
|---|---|---|---|
| 1 | Both DHT11s produce valid readings | `01_`, `03_` | |
| 2 | Board 1 not connected to the AP | `03_` — `association: none, status=6` | |
| 3 | Board 1 sends via ESP-NOW | `03_` — `send : OK (link-layer ack)` | |
| 4 | Board 2 receives | `01_` — `received=634` | |
| 5 | Board 2 deduplicates | `01_` — `duplicates=82, invalid=0` | |
| 6 | Board 2 reads its own sensor | `01_` — `local_ok=yes` | |
| 7 | Combined JSON payload | `01_` — both sensors, one message | |
| 8 | ThingsBoard receives telemetry | screenshots + `02_` MQTT connect | |
| 9 | Logs show node/gateway/ESP-NOW/Wi-Fi/MQTT status | all files | |
| 10 | Losing Board 1 sets `node1_online` false | `05_` | |
| 11 | Restarting Board 1 does not break dedup | `03_` + `04_` + `06_` | |

For criterion 10, quote the threshold crossing directly — it is the cleanest single piece
of evidence in the project:

```
node1_age_ms=11034   node1_online=true
node1_age_ms=21034   node1_online=false
node1_temperature=28.9   ← last known good, still published
```

`06_node_recovery.txt` completes the cycle: Board 1 reconnects with a new `boot_id`,
`node1_online` returns to true, and `duplicates` stays at 0. Read alongside `05_` it gives
the full offline-to-online story in two files.

That capture also contains a useful internal cross-check: `node_uptime=220025 ms` at one
packet every 5 s gives `seq=44`, which is exactly what arrived. Two independent counters on
the sender agreeing means neither the timer nor the sequence logic has drifted.

Include the four ThingsBoard screenshots: both temperature traces, both humidity traces,
Latest telemetry showing all ten keys, and the device list showing State = Active.

---

## 9. Limitations

Name these yourself rather than waiting to be asked.

- **One hop only.** Two radios cannot demonstrate multi-hop forwarding. `ttl` and
  `hop_count` are designed for but unexercised.
- **Sensor resolution.** DHT11 quantises to roughly 1 °C, which is why the dashboard traces
  are staircases rather than curves. The flat sections are quantisation, not a frozen sensor.
- **Sensor self-heating.** Board 2 read consistently warmer than Board 1 early on. Board 2
  runs the enterprise Wi-Fi supplicant and an always-on radio, and its DHT11 sits
  centimetres away on the same breadboard. Partly a real thermal effect, partly part
  tolerance.
- **Unencrypted ESP-NOW and plain MQTT.** No payload encryption, port 1883 not 8883. The
  ThingsBoard access token crosses the network in the clear.
- **Credentials in flash.** Wi-Fi credentials compile into the firmware as plaintext and are
  recoverable over USB. Acceptable for a classroom prototype; unacceptable for deployment.
- **Manual channel configuration.** If the gateway roams to a different AP channel, the mesh
  silently stops working while Wi-Fi and MQTT stay healthy. The gateway detects and warns,
  but recovery is a manual re-upload.

---

## 10. Findings worth reporting as your own

Two things measured rather than assumed:

- **Outbound port 1883 is open on eduroam.** Proved by the MQTT return code: `state=5`
  (not authorised) is a reply *from the broker*, whereas `state=-2` would mean the TCP
  connection never opened. The packets completed a round trip through the campus firewall.
- **WPA2-Enterprise costs ~97 KB of flash.** The gateway sketch went from 69% to 77% of a
  1.31 MB partition when PEAP was enabled. Enterprise authentication is not free.

Also worth noting: eduroam authentication succeeded with the realm `@op.ac.nz` and failed
with `@student.op.ac.nz`. eduroam routes authentication by the domain after the `@`, so the
email domain and the RADIUS realm are not necessarily the same string.

---

## 11. Conclusion

- What was demonstrated, restated against the scope set in section 1.
- What you would do differently or next: more nodes to exercise multi-hop, encrypted
  ESP-NOW, TLS MQTT on 8883, credentials in NVS rather than compiled in.

---

## Appendices

- **A** — Wiring tables (from `README.md` §2)
- **B** — Full serial logs (`evidence/*.txt`)
- **C** — Source code, or a link to it
- **D** — ThingsBoard setup steps and dashboard telemetry keys (from `README.md` §10)

---

## Work split — 50/50

Two halves, each a coherent theme rather than scattered sections, so neither person has to
understand the other's material in depth to write their own.

**Member A — hardware, validation, framing**

| Section | Notes |
|---|---|
| §1 Introduction | Short. Sets the scope claim the whole report is judged against. |
| §2 Hardware | Wiring tables + the three photos. |
| §3 DHT11 substitution | Includes the "did it weaken the demonstration" argument. |
| §8 Testing and results | **The big one.** 11-row criteria table, log quotes, 4 screenshots. |
| §11 Conclusion | Short. Restate against §1. |
| Appendices A, B | Wiring tables, serial logs. |

**Member B — design, implementation, analysis**

| Section | Notes |
|---|---|
| §4 Architecture | Diagram + the single-radio channel constraint. |
| §5 Packet design | 26-byte struct, field justification, why no checksum. |
| §6 Deduplication | Strict last-sequence-wins, and why `boot_id` is load-bearing. |
| §7 Implementation notes | Callback discipline, `setSleep(false)`, `setBufferSize(512)`. |
| §9 Limitations | Six pre-listed. Carries real marks. |
| §10 Findings | Port 1883 on eduroam, 97 KB enterprise-auth cost, realm routing. |
| Appendices C, D | Source code, ThingsBoard setup. |

Roughly even: Member A has one large section (§8) against four short ones; Member B has six
medium sections.

**Shared — agree together, do not split**

- The scope claim in §1 must match §9 and §11. If one person writes "mesh network" and the
  other writes "two-node prototype", that inconsistency is visible and costs marks.
- Formatting, figure numbering, reference style.
- Final read-through: each member proofreads the other's half.
- The contribution statement, if your course requires one.

**Hand-off point:** §8 (Member A) depends on §5 and §6 (Member B) being drafted first —
the results table refers to the packet fields and dedup key by name. Member B should draft
those two sections early so Member A can quote them accurately.

---

## Before submitting

- [ ] Every screenshot legible at print size
- [ ] No real credentials anywhere in the report body — `secrets.h` is gitignored; keep the
      password out of appendices and screenshots too
- [ ] Topology described as a two-node prototype, not a working mesh
- [ ] Pressure never claimed
