"""Generate the pin-level circuit diagram for the P1 project.

The report already carries an architecture block diagram and photographs. This
is the wiring itself: which pin connects to which, in the wire colours actually
used on the bench, so the build can be reproduced from the document alone.

    python tools/make_circuit.py evidence
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
EDGE = "#b8b8b8"
BOARD = "#2b2b2b"
SENSOR = "#3d7ea6"

W_GREEN = "#1f8a4c"
W_ORANGE = "#e07b28"
W_YELLOW = "#d4ad10"

plt.rcParams["font.family"] = "DejaVu Sans"

fig, ax = plt.subplots(figsize=(14, 8.6), dpi=200)
ax.set_xlim(0, 14)
ax.set_ylim(0, 8.6)
ax.axis("off")

ax.text(0.3, 8.25, "Circuit and wiring", fontsize=15,
        fontweight="bold", color=INK)
ax.text(0.3, 7.90, "Wiring is identical on both boards apart from the data pin. "
                   "No electrical connection exists between them.",
        fontsize=10, color=MUTED)
ax.plot([0.3, 13.7], [7.70, 7.70], color=EDGE, linewidth=1.1)


def esp32(x, y, label, data_pin, sublabel):
    """Draw a simplified DevKit showing only the three pins in use."""
    w, h = 2.5, 3.5
    ax.add_patch(FancyBboxPatch((x, y), w, h,
                                boxstyle="round,pad=0.03,rounding_size=0.08",
                                linewidth=1.4, edgecolor="#111111",
                                facecolor=BOARD, zorder=2))
    ax.text(x + w / 2, y + h - 0.32, "ESP32 DevKit", ha="center",
            fontsize=10.5, fontweight="bold", color="white", zorder=3)
    ax.text(x + w / 2, y + h - 0.60, "ESP32-D0WD-V3", ha="center",
            fontsize=7.5, color="#b9c6c4", zorder=3)
    ax.text(x + w / 2, y + 0.24, label, ha="center", fontsize=9,
            fontweight="bold", color="#7fd4c8", zorder=3)
    ax.text(x + w / 2, y - 0.16, sublabel, ha="center", fontsize=7.5,
            color=MUTED, zorder=3)

    ax.add_patch(FancyBboxPatch((x + 0.55, y + 1.60), 1.4, 1.1,
                                boxstyle="round,pad=0.02,rounding_size=0.04",
                                linewidth=0.8, edgecolor="#8a8a8a",
                                facecolor="#c9c9c9", zorder=3))
    ax.text(x + 1.25, y + 2.10, "ESP32", ha="center", fontsize=7.5,
            color="#3a3a3a", zorder=4)

    pins = [("3V3", y + 1.25), ("GND", y + 0.92), (data_pin, y + 0.59)]
    coords = {}
    for name, py in pins:
        ax.add_patch(FancyBboxPatch((x + w - 0.13, py - 0.10), 0.26, 0.20,
                                    boxstyle="round,pad=0.005,rounding_size=0.02",
                                    linewidth=0.7, edgecolor="#7a7a7a",
                                    facecolor="#d8c07a", zorder=4))
        ax.text(x + w - 0.26, py, name, ha="right", va="center",
                fontsize=8.5, fontweight="bold", color="white", zorder=4)
        coords[name] = (x + w + 0.13, py)
    return coords


def dht11(x, y, coords, data_label):
    """Three-pin DHT11 breakout, wired back to the DevKit."""
    w, h = 1.5, 2.2
    ax.add_patch(FancyBboxPatch((x, y), w, h,
                                boxstyle="round,pad=0.03,rounding_size=0.06",
                                linewidth=1.3, edgecolor="#1d4d66",
                                facecolor=SENSOR, zorder=2))
    ax.text(x + w / 2, y + h - 0.28, "DHT11", ha="center", fontsize=10,
            fontweight="bold", color="white", zorder=3)
    ax.text(x + w / 2, y + h - 0.54, "3-pin module", ha="center", fontsize=7.5,
            color="#dbeaf2", zorder=3)

    for k in range(4):
        ax.plot([x + 0.38, x + w - 0.38], [y + 1.20 - k * 0.11] * 2,
                color="#2a5f7d", linewidth=2.4, zorder=3)

    pads = [("+", y + 0.72, W_GREEN, "3V3"),
            ("OUT", y + 0.46, W_YELLOW, data_label),
            ("-", y + 0.20, W_ORANGE, "GND")]
    for name, py, colour, target in pads:
        ax.add_patch(FancyBboxPatch((x - 0.13, py - 0.09), 0.26, 0.18,
                                    boxstyle="round,pad=0.005,rounding_size=0.02",
                                    linewidth=0.7, edgecolor="#7a7a7a",
                                    facecolor="#d8c07a", zorder=4))
        ax.text(x + 0.22, py, name, ha="left", va="center", fontsize=8,
                fontweight="bold", color="white", zorder=4)
        sx, sy = coords[target]
        ax.add_patch(FancyArrowPatch((sx, sy), (x - 0.13, py),
                                     arrowstyle="-", linewidth=2.8,
                                     color=colour, zorder=3,
                                     connectionstyle="arc3,rad=-0.20"))


ax.text(0.3, 7.32, "BOARD 1  -  Remote sensor node", fontsize=11,
        fontweight="bold", color=ACCENT)
c1 = esp32(0.5, 3.2, "node_id = 1", "D4",
           "MAC 44:1D:64:F5:FA:24  |  never joins Wi-Fi")
dht11(4.3, 3.9, c1, "D4")

ax.text(7.6, 7.32, "BOARD 2  -  Gateway + local sensor", fontsize=11,
        fontweight="bold", color=ACCENT)
c2 = esp32(7.8, 3.2, "root_id = 0", "D5",
           "MAC 44:1D:64:F4:F1:C8  |  joins eduroam")
dht11(11.6, 3.9, c2, "D5")

ax.add_patch(FancyArrowPatch((3.05, 6.88), (10.30, 6.88),
                             arrowstyle="-|>", mutation_scale=16,
                             linewidth=1.8, color=ACCENT,
                             linestyle=(0, (5, 3)), zorder=5))
ax.text(6.67, 7.04, "ESP-NOW  -  encrypted, PMK + per-peer LMK", ha="center",
        fontsize=9.5, fontweight="bold", color=ACCENT)
ax.text(6.67, 6.56, "26-byte packet every 5 s   |   no wire, no shared ground",
        ha="center", fontsize=8, color=MUTED)

for x0 in (0.5, 7.8):
    ax.add_patch(FancyBboxPatch((x0 + 0.85, 2.30), 0.80, 0.42,
                                boxstyle="round,pad=0.02,rounding_size=0.05",
                                linewidth=1.0, edgecolor=EDGE,
                                facecolor="#f2f2f2", zorder=2))
    ax.text(x0 + 1.25, 2.51, "USB-C", ha="center", va="center", fontsize=8,
            color=MUTED, zorder=3)
    ax.plot([x0 + 1.25, x0 + 1.25], [2.72, 3.20], color=MUTED,
            linewidth=1.4, zorder=1)

ax.text(7.0, 2.02, "Each board is powered from its own USB supply; "
                   "the two supplies are not common.",
        ha="center", fontsize=8.5, color=MUTED)

ax.add_patch(FancyBboxPatch((0.5, 0.30), 6.2, 1.45,
                            boxstyle="round,pad=0.02,rounding_size=0.06",
                            linewidth=1.0, edgecolor=EDGE,
                            facecolor="#fbfbfb", zorder=2))
ax.text(0.78, 1.50, "Connections  (identical on both boards)", fontsize=9.5,
        fontweight="bold", color=INK, zorder=3)
rows = [(W_GREEN, "green", "3V3", "+"),
        (W_ORANGE, "orange", "GND", "-"),
        (W_YELLOW, "yellow", "D4 / D5", "OUT")]
for i, (colour, name, a, b) in enumerate(rows):
    yy = 1.18 - i * 0.28
    ax.plot([0.85, 1.30], [yy, yy], color=colour, linewidth=3.4, zorder=3)
    ax.text(1.45, yy, "%-7s ESP32 %-8s ->  DHT11 %s" % (name, a, b),
            va="center", fontsize=8.5, color=MUTED,
            family="DejaVu Sans Mono", zorder=3)

ax.add_patch(FancyBboxPatch((7.1, 0.30), 6.6, 1.45,
                            boxstyle="round,pad=0.02,rounding_size=0.06",
                            linewidth=1.0, edgecolor=EDGE,
                            facecolor="#fbfbfb", zorder=2))
ax.text(7.38, 1.50, "Notes", fontsize=9.5, fontweight="bold", color=INK,
        zorder=3)
notes = [
    "The 3-pin module carries its own data-line pull-up, so no resistor is needed.",
    "Sensors run at 3.3 V. Nothing is connected to VIN or EN.",
    "GPIO4 on Board 1, GPIO5 on Board 2 - the only wiring difference.",
    "Each DevKit straddles the breadboard centre gap so pin rows are not shorted.",
]
for i, line in enumerate(notes):
    ax.text(7.38, 1.20 - i * 0.26, "-  " + line, fontsize=8.2, color=MUTED,
            zorder=3)

path = os.path.join(OUT, "fig_circuit_diagram.png")
fig.savefig(path, bbox_inches="tight", facecolor="white")
plt.close(fig)
print("wrote %s" % path)
