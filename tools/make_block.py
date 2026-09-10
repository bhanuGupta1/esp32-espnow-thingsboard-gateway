"""Generate the system block diagram for the P1 project.

The circuit diagram answers "which pin connects to which". This answers "what
moves, and over what". The three tiers are drawn as columns so the single
crossing point - the gateway - is visually obvious, because the whole argument
of the project is that one board carries the other's data.

    python tools/make_block.py evidence
"""

import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(OUT, exist_ok=True)

INK = "#1a1a1a"
MUTED = "#666666"
ACCENT = "#0b6b62"
EDGE = "#c2c2c2"
NODE_C = "#2b3a42"
GW_C = "#1d4d47"
CLOUD_C = "#3d6ea6"
SENSOR_C = "#3d7ea6"
RADIO = "#c2601c"

plt.rcParams["font.family"] = "DejaVu Sans"

fig, ax = plt.subplots(figsize=(14, 8.2), dpi=200)
ax.set_xlim(0, 14)
ax.set_ylim(0, 8.2)
ax.axis("off")

ax.text(0.3, 7.85, "System block diagram", fontsize=15, fontweight="bold",
        color=INK)
ax.text(0.3, 7.50, "Three tiers. Board 2 is the only device on both networks, "
                   "which is what lets Board 1 reach the cloud without joining it.",
        fontsize=10, color=MUTED)
ax.plot([0.3, 13.7], [7.30, 7.30], color=EDGE, linewidth=1.1)

# Tier bands, drawn first so everything else sits on top.
tiers = [(0.5, 4.0, "SENSING TIER", "no network credentials"),
         (5.1, 4.0, "EDGE TIER", "aggregates, buffers, controls"),
         (9.7, 3.8, "CLOUD TIER", "storage, dashboards, commands")]
for x, w, name, sub in tiers:
    ax.add_patch(FancyBboxPatch((x, 0.95), w, 5.95,
                                boxstyle="round,pad=0.03,rounding_size=0.08",
                                linewidth=1.0, edgecolor=EDGE,
                                facecolor="#fafafa", zorder=1))
    ax.text(x + w / 2, 6.62, name, ha="center", fontsize=10,
            fontweight="bold", color=ACCENT, zorder=2)
    ax.text(x + w / 2, 6.32, sub, ha="center", fontsize=8, color=MUTED,
            zorder=2)


def box(x, y, w, h, fill, title, lines, title_col="white", body_col="#d8e4e2"):
    ax.add_patch(FancyBboxPatch((x, y), w, h,
                                boxstyle="round,pad=0.03,rounding_size=0.07",
                                linewidth=1.2, edgecolor="#0d0d0d",
                                facecolor=fill, zorder=3))
    ax.text(x + w / 2, y + h - 0.34, title, ha="center", fontsize=10.5,
            fontweight="bold", color=title_col, zorder=4)
    for i, line in enumerate(lines):
        ax.text(x + w / 2, y + h - 0.72 - i * 0.30, line, ha="center",
                fontsize=8.2, color=body_col, zorder=4)


# --- Sensing tier ---------------------------------------------------------
box(0.85, 4.55, 3.3, 1.45, SENSOR_C, "DHT11", ["temperature, humidity",
                                               "1 Hz max sample rate"])
box(0.85, 1.55, 3.3, 2.45, NODE_C, "BOARD 1  -  node", [
    "ESP32, station mode",
    "never associates with an AP",
    "reads GPIO4 every 5 s",
    "builds a 26-byte packet",
    "seq + boot_id per packet",
])

# --- Edge tier ------------------------------------------------------------
box(5.45, 4.55, 3.3, 1.45, SENSOR_C, "DHT11", ["the gateway's own sensor",
                                               "read from GPIO5"])
box(5.45, 1.55, 3.3, 2.45, GW_C, "BOARD 2  -  gateway", [
    "ESP32, joins eduroam (PEAP)",
    "validates and deduplicates",
    "merges both sensor sources",
    "12-slot store-and-forward ring",
    "handles RPC from the cloud",
])

# --- Cloud tier -----------------------------------------------------------
box(10.05, 4.35, 3.1, 1.85, CLOUD_C, "ThingsBoard", [
    "device registry",
    "time-series storage",
    "rule engine, alarms",
])
box(10.05, 1.55, 3.1, 2.25, CLOUD_C, "Dashboards", [
    "16 telemetry keys",
    "both temperature traces",
    "3 RPC command buttons",
])

# --- Sensor wires (one-way, inside a board) -------------------------------
for x in (2.5, 7.1):
    ax.add_patch(FancyArrowPatch((x, 4.55), (x, 4.02),
                                 arrowstyle="-|>", mutation_scale=13,
                                 linewidth=1.5, color=MUTED, zorder=5))
ax.text(2.5, 4.27, " 1-wire", fontsize=7.5, color=MUTED, ha="left", zorder=6)
ax.text(7.1, 4.27, " 1-wire", fontsize=7.5, color=MUTED, ha="left", zorder=6)

# --- ESP-NOW hop ----------------------------------------------------------
ax.add_patch(FancyArrowPatch((4.15, 2.95), (5.45, 2.95),
                             arrowstyle="-|>", mutation_scale=16,
                             linewidth=2.2, color=RADIO, zorder=6,
                             linestyle=(0, (4, 2.5))))
ax.text(4.80, 3.36, "ESP-NOW", ha="center", fontsize=9.5, fontweight="bold",
        color=RADIO, zorder=6)
ax.text(4.80, 3.10, "encrypted", ha="center", fontsize=7.5, color=RADIO,
        zorder=6)
ax.text(4.80, 2.62, "26 B / 5 s", ha="center", fontsize=7.5, color=MUTED,
        zorder=6)
ax.text(4.80, 2.38, "acked, ~1 ms", ha="center", fontsize=7.5, color=MUTED,
        zorder=6)

# --- MQTT, both directions ------------------------------------------------
ax.add_patch(FancyArrowPatch((8.75, 3.25), (10.05, 3.25),
                             arrowstyle="-|>", mutation_scale=16,
                             linewidth=2.0, color=ACCENT, zorder=6))
ax.text(9.40, 3.50, "telemetry", ha="center", fontsize=8.5, color=ACCENT,
        zorder=6)
ax.text(9.40, 3.02, "JSON / 10 s", ha="center", fontsize=7.5, color=MUTED,
        zorder=6)

ax.add_patch(FancyArrowPatch((10.05, 2.35), (8.75, 2.35),
                             arrowstyle="-|>", mutation_scale=16,
                             linewidth=2.0, color="#8a4fbf", zorder=6))
ax.text(9.40, 2.58, "RPC commands", ha="center", fontsize=8.5,
        color="#8a4fbf", zorder=6)
ax.text(9.40, 2.10, "MQTT  port 1883", ha="center", fontsize=7.5, color=MUTED,
        zorder=6)

# --- Footnotes ------------------------------------------------------------
ax.add_patch(FancyBboxPatch((0.5, 0.18), 13.2, 0.62,
                            boxstyle="round,pad=0.02,rounding_size=0.05",
                            linewidth=1.0, edgecolor=EDGE,
                            facecolor="#fbfbfb", zorder=2))
ax.text(0.78, 0.60, "Both ESP32s share one 2.4 GHz channel, which the access "
                    "point chooses for Board 2 and Board 1 must be told. "
                    "Board 1 holds no Wi-Fi credentials at all.",
        fontsize=8.3, color=MUTED, zorder=3)
ax.text(0.78, 0.34, "The ttl and hop_count fields exist in the packet for "
                    "forwarding, but with two radios nothing is ever "
                    "forwarded: this is one hop, mesh-ready rather than a mesh.",
        fontsize=8.3, color=MUTED, zorder=3)

path = os.path.join(OUT, "fig_block_diagram.png")
fig.savefig(path, bbox_inches="tight", facecolor="white")
plt.close(fig)
print("wrote %s" % path)
