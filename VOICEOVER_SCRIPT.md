# Voiceover script — `evidence/P1_demo_cloud_and_hardware.mp4`

Timed against the assembled video, which runs **1:49.1**. Each block gives the window it has
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

## 0:57 – 1:01 · "At the bench" card *(10 words, ~4 s)*

> That is the cloud view. This is the bench it comes from.

---

## 1:01 – 1:16 · Dashboard beside the boards *(36 words, ~15 s)*

> Here is the same dashboard running next to the hardware. Node one communication age was
> counting in milliseconds while this was filmed, so the board on the desk is the one feeding
> that number.

---

## 1:16 – 1:25 · Board 1 disconnected *(22 words, ~9 s)*

> Now I pull Board one's power. The gateway keeps publishing its own readings, and after twenty
> seconds node one online goes false.

*The dashboard flipping to false is not in this footage — the log evidence is in section 8.5.
Do not claim on camera that the video shows it.*

---

## 1:25 – 1:29 · Recap card *(13 words, ~4 s)*

> The same round trip once more, this time with the counters in view.

---

## 1:29 – 1:44 · RPC round trip *(36 words, ~15 s)*

> Publish interval is two thousand milliseconds, not the ten thousand compiled into the
> firmware, and the handled count has incremented. Out-of-range values are refused rather than
> clamped, so a bad command cannot quietly take effect.

---

## 1:44 – 1:49 · Closing card *(30 words, ~5 s — read briskly)*

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
| The dashboard **after** the 20 s threshold | The disconnection is filmed, but the payoff is not. Film the screen showing `node1_online` false and the last known temperature still published |
| Plugging Board 1 back in | New `boot_id`, sequence restarts, accepted with no duplicate flood — §8.6 |
| Warming the sensor by hand | The whole chain — sensor, radio, gateway, MQTT, cloud — reacting in about fifteen seconds |

The first of these is the one worth getting. Without it the video shows a cable being pulled
and asks the viewer to take the consequence on trust, which is exactly the kind of claim the
rest of this portfolio avoids making.

Send me those clips and I will cut them into the same timeline.
