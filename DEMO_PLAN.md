# Demo runsheet — 10 minutes + 5 minutes Q&A

Worth **15 marks** under *Presentation & Defense*. The top band requires "excellent demo
and confident, well-informed responses in Q&A" — so the demo has to work *and* you have to
be able to defend the design choices behind it.

---

## Before you walk in

**The night before**

- [ ] Both boards flashed with final firmware (sketch 04 on Board 1, sketch 05 on Board 2)
- [ ] `SEND_DUPLICATE_FOR_DEMO` is `0` in sketch 04 — you will flip it live, see Act 4
- [ ] ThingsBoard dashboard open in a browser tab, logged in
- [ ] Serial monitors ready for both COM ports
- [ ] Phone hotspot configured as a **fallback** in case eduroam misbehaves in the room

**In the room, 5 minutes before**

- [ ] Power both boards
- [ ] Run the channel check:
      `.\build.ps1 -Monitor -NoReset -Port COM4 -Seconds 15`
- [ ] `espnow_received_count` climbing → good. Frozen → the gateway landed on a different
      eduroam access point. Read the channel from the boot banner, put it in sketch 04,
      re-upload to Board 1. Two minutes.
- [ ] Confirm telemetry is arriving in ThingsBoard

**Screen layout.** Three windows visible at once: Board 1 serial, Board 2 serial, ThingsBoard
dashboard. The whole argument of this project is that data crosses from one to the other, and
the audience needs to see all three to follow it.

---

## The 10 minutes

### Act 1 — What it is (1 min)

Hold up the two boards. Physically separate them on the desk.

> "Two ESP32 boards. No wire between them. This one reads temperature and humidity and sends
> it by radio using ESP-NOW. This one reads its own sensor, receives the first board's data,
> merges both, and publishes to the cloud over MQTT."

Say the scope claim out loud, early:

> "This is a two-node prototype with a mesh-ready packet design. With two radios we can only
> demonstrate one hop — the packet carries TTL and hop count fields, but nothing is ever
> forwarded, because there is nowhere to forward it to."

Saying this yourself is worth more than being asked. It signals you understand the limit
rather than hoping nobody notices.

### Act 2 — The architecture (2 min)

Put the architecture diagram on screen. Cover the five knowledge domains explicitly — the
marking scheme allocates 25 marks to them, so name them as you go:

| Domain | What to say |
|---|---|
| **Networking** | ESP-NOW is connectionless, link-layer acknowledged, no IP stack. MQTT over TCP for the cloud leg. Two protocols, chosen for different jobs. |
| **Hardware** | ESP32-D0WD-V3, single 2.4 GHz radio — that constraint drives the whole design. DHT11 on one GPIO. |
| **Software** | Callback runs on the Wi-Fi task, work happens in `loop()`, FreeRTOS queue between them. |
| **Cloud** | ThingsBoard, MQTT 1883, access token as username, telemetry topic. |
| **Security** | Say what is and is not protected — see the security section of the portfolio. Do not skip this one. |

**The single-radio constraint is the best thing you have to explain.** An ESP32 has one
radio, so one channel. The gateway must join Wi-Fi for the cloud, and the access point
dictates its channel. So the channel is *discovered*, not chosen — which is why the gateway
must be flashed and running before the node can even be configured.

### Act 3 — Live data (2 min)

Point at Board 1's serial:

```
[NODE] Wi-Fi association: none (by design), status=6
[NODE] tx seq=42 temperature=21.4 C humidity=48.0 %
[NODE] send to 44:1D:64:F4:F1:C8 : OK (link-layer ack received)
```

> "Status 6 is disconnected — this board has no network connection at all. And that OK is a
> real acknowledgement from the other board's MAC layer, not just a queued send."

Point at Board 2's serial, then the dashboard. Same numbers in all three places.

**Then the moment that sells it: put your thumb on Board 1's sensor.**

Temperature climbs roughly 21 °C → 28 °C over about 15 seconds and the humidity drops.
Watch it move on the ThingsBoard chart. Take your thumb off and it relaxes back.

That single gesture proves the entire chain — sensor, ESP-NOW, gateway, MQTT, cloud — in
fifteen seconds, and it is far more convincing than any static screenshot.

### Act 4 — Deduplication (2 min)

Explain the key first: `(src_id, boot_id, seq)`.

> "`boot_id` is randomised every power-up. Without it, restarting the node would look
> identical to a replay attack — every packet would have a sequence number below the stored
> baseline and would be rejected forever. The node would never come back."

Two ways to show it:

**Safe option (recommended):** have a second board pre-flashed with `SEND_DUPLICATE_FOR_DEMO 1`
and swap it in. Or show the saved log `evidence/01_gateway_running_state.txt`:
`received=634 duplicates=82 invalid=0`.

**Live option:** re-upload with the flag on. Takes about 90 seconds and needs a working
laptop connection. Only do this if you have rehearsed it.

Then unplug Board 1 and keep talking. After 20 seconds:

```
node1_age_ms=21034   node1_online=false
```

> "The last known temperature is still published — the boolean carries the truth about
> freshness. That way the dashboard chart stays continuous instead of gapping, but nobody is
> misled into thinking stale data is current."

Plug it back in. New `boot_id`, sequence restarts at 1, accepted immediately, no duplicate flood.

### Act 5 — Cloud (2 min)

ThingsBoard dashboard. Both temperature traces on one chart.

> "Two physically separate sensors. One of them has no network connection of its own. They
> arrive as a single cloud stream because the gateway merges them at the edge — before the
> data ever leaves the local network."

Show the device page: State = **Active**, and Latest Telemetry with all ten keys.

### Act 6 — Limits and close (1 min)

Name your own limitations before anyone asks. Pick three:

- One hop only — cannot demonstrate multi-hop with two radios
- Unencrypted ESP-NOW, plain MQTT on 1883, token crosses the network in clear text
- Credentials compiled into firmware, recoverable over USB

> "Next steps would be a third node to actually exercise the TTL field, encrypted ESP-NOW,
> TLS MQTT on 8883, and credentials provisioned into NVS at first boot rather than compiled in."

---

## Q&A — 5 minutes

Prepare these. The marking scheme rewards "confident, well-informed responses", and these are
the questions the design invites.

**"Why ESP-NOW instead of Wi-Fi or Bluetooth?"**
No association, no DHCP, no IP stack. A node can send a reading and go back to sleep in
milliseconds rather than the seconds a Wi-Fi association takes. Lower power, lower latency,
and it works with no access point at all. The trade is no routing and no internet.

**"Why does the node not connect to Wi-Fi?"**
One radio, one channel. If it associated it would negotiate its own channel and the two boards
could drift apart whenever the router moved either of them. Station mode without association
gives a working radio and manual channel control.

**"How do you know packets actually arrive?"**
ESP-NOW unicast is acknowledged at the 802.11 MAC layer. The send callback reports that ack.
Had we only checked `esp_now_send()`'s return value we would be reporting "queued", not
"delivered" — a distinction that would have hidden every failure.

**"What happens if two packets arrive out of order?"**
They do not, over an acknowledged two-radio link. We chose strict last-sequence-wins
deliberately. A forward jump is treated as a gap, not a duplicate — a receiver that demanded
contiguous sequence numbers would deadlock after the first lost packet.

**"Is it secure?"** — *expect this one; Security is 25 marks*
Honestly: not for deployment. ESP-NOW is unencrypted, so anyone with an SDR in range can read
the readings and could inject packets that pass validation. MQTT is on 1883, so the access
token crosses in clear text. Credentials are compiled into flash and recoverable over USB.
What *is* in place: strict packet validation (length, version, type, addressing, range checks),
and deduplication that rejects replayed packets. Production would need ESP-NOW PMK/LMK
encryption, TLS on 8883, and NVS-provisioned credentials.

**"Why DHT11 and not the BMP280 in the brief?"**
Documented substitution — that hardware was not available. We measure temperature and
humidity, not pressure. It cost us I²C experience and chart resolution, but gained a
demonstrable variable: indoor pressure would be a flat line for hours, humidity responds to a
hand in seconds.

**"What was hardest?"**
The eduroam authentication. `status=6` is returned identically for a wrong realm, a wrong
password, and an out-of-range AP — no diagnostic distinguishes them. It turned out
`@student.<institution>` is a valid email domain but not a registered RADIUS realm;
`@<institution>` worked. Found by controlled experiment, not by reasoning.

**"How would you scale this to 50 nodes?"**
The dedup state is a single slot — it would become an array indexed by `src_id`. The packet
format already carries addressing and TTL so it would not have to change. The real limits
would be ESP-NOW's peer table (20 encrypted peers) and channel contention.

---

## If something breaks

**No telemetry.** Check the gateway serial first. Wi-Fi down, MQTT down, or no packets
arriving are three different failures with three different log lines. Say which one it is out
loud — diagnosing confidently in front of the room reads better than a demo that never fails.

**Node offline.** Almost always the channel. Show the boot banner, explain the constraint, and
point at the saved evidence logs instead. **This is why you have captured logs** — the story
survives a hardware failure.

**Nothing works at all.** Fall back entirely to `evidence/` and the dashboard screenshots. The
work is documented; the demo is a presentation of it, not the only proof it exists.
