# ThingsBoard configuration

Cloud-side work. No firmware changes, no hardware. Roughly 20 minutes.

This exists because **Cloud platform is one of the five assessed knowledge domains**, and a
device that only publishes telemetry uses almost none of what a cloud IoT platform actually
provides. Adding alarms and control demonstrates the rule engine and the downlink path, which
is the difference between using ThingsBoard as a database and using it as a platform.

---

## 1. Alarms — the rule engine

Alarms are ThingsBoard's headline feature and the clearest evidence of cloud-tier capability.
Three are worth creating; each takes about three minutes.

### Setup

**Entities → Device profiles → default → Alarm rules → Add alarm rule**

### Alarm A — remote node offline

| Field | Value |
|---|---|
| Alarm type | `Node offline` |
| Severity | **Major** |
| Condition | Key `node1_online` (boolean) **equals** `false` |
| Clear condition | Key `node1_online` **equals** `true` |

This is the one to demonstrate live: unplug Board 1, wait twenty seconds, and the alarm raises
itself in the ThingsBoard alarm table without anyone touching the dashboard. Plug it back in
and it clears.

### Alarm B — telemetry buffering

| Field | Value |
|---|---|
| Alarm type | `Cloud link degraded` |
| Severity | **Warning** |
| Condition | Key `buffered_now` (numeric) **greater than** `0` |
| Clear condition | `buffered_now` **equals** `0` |

Fires when the gateway starts queueing because the broker is unreachable. Worth having because
it detects a problem the telemetry itself cannot show — if data is not arriving, nothing
arrives to say so. The alarm raises on the *next successful* publish, when the queue drains.

### Alarm C — sensor fault

| Field | Value |
|---|---|
| Alarm type | `Gateway sensor fault` |
| Severity | **Minor** |
| Condition | Key `gateway_sensor_ok` **equals** `false` |
| Clear condition | `gateway_sensor_ok` **equals** `true` |

Distinguishes "the sensor is broken" from "the device is offline" — two failures that look
identical on a chart but need different responses.

### Why this matters for the report

Alarms move detection from the observer to the platform. Without them, a fault is only found
when somebody happens to look at a chart. This is the practical difference between monitoring
and observability, and it is worth one sentence in §7 of the portfolio.

---

## 2. RPC control widgets — the downlink

The gateway subscribes to `v1/devices/me/rpc/request/+` and implements three methods. Adding
control widgets proves the link is bidirectional.

**Open your dashboard → Edit mode → Add widget → search "button" → Buttons bundle →
Command button.**

On ThingsBoard 4.3.1 the widget you want is called **Command button** and lives in the
**Buttons** bundle. Older guides call it "RPC button" under "Control widgets"; that name no
longer appears in the widget picker. Each button then needs, in the Basic tab:

| Field | Where | Value |
|---|---|---|
| Target device | top of the dialog | `P1 Gateway` |
| On click | Behavior → pencil icon | Action **Execute RPC**, then set Method |
| Label | Appearance | whatever the button should read |

**One trap worth knowing.** Under *Parameters*, the type defaults to **Boolean**. The firmware
finds `"params"` and scans forward to the first digit or sign, so a Boolean `true` parses as
`0` and the command is **rejected** by the range check. Set the type to **Integer** for
`setPublishInterval`. Methods that take no argument can be left as **None**.

### Widget 1 — `getStatus`

| Field | Value |
|---|---|
| Method | `getStatus` |
| Parameters | *None* |

Returns liveness, counters, queue depth, current interval and radio channel. Useful in the
demo because it proves the device answers, not just speaks.

### Widget 2 — `setPublishInterval`

Create two buttons rather than a text field — faster and less error-prone to demonstrate.

| Button label | Method | Parameters (Integer) |
|---|---|---|
| "Fast publish (2 s)" | `setPublishInterval` | `2000` |
| "Normal publish (10 s)" | `setPublishInterval` | `10000` |

Press "Fast" during the demo and the telemetry cadence visibly changes on the chart within
seconds. That is the single most convincing thing you can show about cloud control.

**Also worth demonstrating:** send `1000`. The device **rejects** it and replies with the valid
range, because the minimum is 2000 ms. Showing a rejected command is better evidence of
thought than showing an accepted one — it proves the device validates input rather than obeying
blindly.

### Widget 3 — `resetCounters`

| Field | Value |
|---|---|
| Method | `resetCounters` |
| Parameters | *None* |

Zeroes the diagnostic counters without a reboot. Handy immediately before the demo so the
numbers start clean.

---

## 3. Dashboard additions

Beyond the two existing charts, four widgets are worth adding:

| Widget | Keys | Why |
|---|---|---|
| **Alarm table** | — | Shows the rule engine working. Put it prominently. |
| LED indicator | `node1_online` | Red/green, instantly readable |
| LED indicator | `gateway_sensor_ok` | Distinguishes sensor fault from node loss |
| Card / label | `buffered_now`, `dropped_total`, `rpc_handled` | Operational metrics beside the data |

The alarm table is the important one. It is visual proof of cloud-side logic rather than
cloud-side storage.

---

## 4. Suggested demo sequence

Slots into `DEMO_PLAN.md` between Act 5 (cloud) and Act 6 (limits):

1. Show the dashboard with both temperature traces — the existing content
2. Press **getStatus** — device replies with live counters
3. Press **Fast publish (2 s)** — watch the chart cadence change
4. Send `1000` — device **rejects** it, states the valid range
5. Unplug Board 1 — after 20 s the **alarm raises itself**
6. Plug it back in — alarm **clears itself**
7. Press **Normal publish (10 s)** to restore

Steps 4 and 5 are the strongest. One shows input validation; the other shows the platform
detecting a fault without a human watching.

---

## 5. Screenshots to capture for the portfolio

| Screenshot | Status |
|---|---|
| Command buttons on the dashboard | ✅ `dashboard_06_full_system_live.jpg` |
| Latest telemetry showing `rpc_handled` and the changed `publish_interval_ms` | ✅ `dashboard_05_rpc_publish_interval_2000.jpg` |
| Latest telemetry showing the live `node1_*` keys | ✅ `dashboard_07_node1_keys_live.jpg` |
| Alarm rules configuration page | Still to capture |
| Alarm table with `Node offline` **active** (Board 1 unplugged) | Still to capture |
| Same table showing it **cleared** (Board 1 restored) | Still to capture |
| An RPC response showing a **rejected** `setPublishInterval` | Still to capture |

The three captured ones are already embedded in the report as Figures 9–11. The alarm
screenshots depend on the alarm rules being configured first (§1), and the rejection
screenshot needs a `1000` sent from a fourth button or the REST API.
