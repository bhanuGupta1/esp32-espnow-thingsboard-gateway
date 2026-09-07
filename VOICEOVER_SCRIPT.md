# Voiceover script — `evidence/P1_demo_cloud_and_hardware.mp4`

Timed against the assembled video, which runs **1:20.9**. Each block gives the window it has
to fit in and a word count. Read at a normal pace, around 2.5 words a second, and you will
land inside every window with room to breathe.

Record it in one take if you can. Pauses between blocks are fine — silence over a card looks
deliberate, a rushed sentence does not.

---

## 0:00 – 0:05 · Title card *(12 words, ~5 s)*

> Project one: an ESP-NOW sensor mesh with a cloud bridge, built on two ESP32 boards.

---

## 0:05 – 0:20 · Your hardware clip *(37 words, ~15 s)*

> These are the two boards. Each has its own DHT11 sensor and its own USB power, and there is
> no wire between them. Board one never joins Wi-Fi at all — the only link is 2.4 gigahertz
> radio.

*Pause on the last sentence; it is the claim the whole project rests on.*

---

## 0:20 – 0:24 · Cloud section card *(11 words, ~4 s)*

> Board two joins eduroam and publishes both sensors to ThingsBoard over MQTT.

---

## 0:24 – 0:57 · Captioned walkthrough *(80 words, ~33 s)*

> The gateway is online. Node one communication age is counting in milliseconds, which means
> Board one is reaching it right now over the radio.
>
> The gateway also reads its own sensor, so two sources are merged at the edge before anything
> leaves the network.
>
> Now the other direction. Pressing this button sends an RPC down to the device. The publish
> interval changes from ten seconds to two, and the device reports that change back in its own
> telemetry — so the round trip is confirmed at both ends.
>
> The node keys come from a board with no network connection of its own.

---

## 0:57 – 1:01 · Recap card *(13 words, ~4 s)*

> The same round trip once more, this time with the counters in view.

---

## 1:01 – 1:16 · RPC round trip *(36 words, ~15 s)*

> Publish interval is two thousand milliseconds, not the ten thousand compiled into the
> firmware, and the handled count has incremented. Out-of-range values are refused rather than
> clamped, so a bad command cannot quietly take effect.

---

## 1:16 – 1:21 · Closing card *(30 words, ~5 s — read briskly)*

> Two boards, no wire, one cloud stream: edge aggregation, deduplication, encrypted ESP-NOW,
> and control in both directions.

---

## Recording notes

- **Phone voice recorder is fine.** Hold it about 20 cm away and slightly off to the side of
  your mouth, so plosives do not thump the microphone.
- **Room matters more than the microphone.** A room with soft furnishings beats a hard-walled
  lab. Close the window and turn off any fan.
- **No background music.** It competes with speech, earns no marks, and a licensed track on a
  submitted artefact is a copyright problem you do not need.
- **Retakes are free.** If a sentence goes wrong, pause for two seconds and say it again — the
  bad take is easy to cut out.
- Save as `.m4a`, `.mp3` or `.wav` anywhere convenient and tell me the path. I will mux it onto
  the video and adjust the segment lengths if your delivery runs long or short.

## What still needs filming

This video covers the boards at rest and the whole cloud tier. Three shots from
`SUBMISSION_CHECKLIST.md` §3 are still missing, and they are the ones that prove the system
responds to the physical world:

| Shot | Why it matters |
|---|---|
| Warming the sensor by hand | Shows the whole chain — sensor, radio, gateway, MQTT, cloud — reacting in about fifteen seconds |
| Unplugging Board 1 | `node1_online` flips false after the 20 s threshold; proves the stale detection in §8.5 |
| Plugging it back in | New `boot_id`, sequence restarts, accepted with no duplicate flood — §8.6 |

Send me those clips and I will cut them into the same timeline.
