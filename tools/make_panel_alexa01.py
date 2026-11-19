#!/usr/bin/env python3
"""Draws res/Alexa01.svg and writes src/Layout01.hpp with the same positions.

Alexa01 is one voice of Alexa02 with every setting on its own knob.
Labels are outline paths (Rack can't draw SVG <text>). Positions in mm.
    pip install fonttools
    python3 tools/make_panel_alexa01.py
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

HP = 42
W, H = HP * 5.08, 128.5
BG = "#24272C"
BG2 = "#2F333A"
INK = "#ECE6D8"
SOFT = "#9AA3AE"
ACCENT = "#F2A541"
OUTBOX = "#15171A"
LINE = "#3A3F47"

CTRL = 3.2   # control labels (project standard)
JACK = 2.9   # jack labels

# ---------------------------------------------------------------- layout
# the 32 knob slots, in the order src/Alexa01.cpp uses
SLOT_LABELS = ["TUNE", "FINE", "WAVE", "PWM", "NOISE", "MIX",
               "ATTACK", "DECAY", "OPEN", "SLOPE", "REPEAT", "ACCENT",
               "DECAY", "SLOPE", "AMOUNT", "TYPE", "LEVEL", "PITCH",
               "TUNE 2", "FM 2", "WAVE 2", "TUNE 3", "FM 3", "WAVE 3", "FM/MIX",
               "CUTOFF", "RESO", "TYPE", "DRIVE", "LEVEL", "S.RATE", "DRIVE"]
ROWS = [[("OSC", range(0, 6))],
        [("AMP", range(6, 12))],
        [("PITCH", range(12, 15)), ("CLICK", range(15, 18))],
        [("FM", range(18, 25))],
        [("FILTER", range(25, 29)), ("MIX", range(29, 32))]]
ROW_TOP0, ROW_PITCH, ROW_H = 13.0, 20.5, 19.0
STRIP, KNOB_PITCH, LEFT = 6.0, 16.5, 4.0

slot_pos = [None] * 32
boxes = []   # (x, y, w, h, title)
for r, row in enumerate(ROWS):
    top = ROW_TOP0 + r * ROW_PITCH
    x = LEFT
    for title, slots in row:
        w = STRIP + len(slots) * KNOB_PITCH
        boxes.append((x, top, w, ROW_H, title))
        for i, s in enumerate(slots):
            slot_pos[s] = (x + STRIP + KNOB_PITCH * (i + 0.5), top + 12.8)
        x += w + 1.2

RX0, RX1 = 141.0, W - 4.2
RCX = (RX0 + RX1) / 2
DISP = (RX0, 11.0, RX1 - RX0, 26.0)
TYPE_Y = 49.0
TYPE_X = [RX0 + (RX1 - RX0) / 8 * (2 * k + 1) for k in range(4)]
TYPE_NAMES = ["DRUM", "SNARE", "CYMBAL", "HAT"]
MEM_Y = 64.0
MEM_X = [RX0 + (RX1 - RX0) / 12 * (2 * k + 1) for k in range(6)]
ACT_Y = 78.0
ACT_X = TYPE_X
ACT_NAMES = ["STORE", "RECALL", "MUTATE", "PLAY"]
MORPH_KNOB = (RX0 + 13.0, 96.0)
MODE_SW = (RX0 + 33.0, 96.0)
MORPH_ATT = (RX1 - 10.5, 91.0)
MORPH_CV = (RX1 - 10.5, 101.5)
JACK_Y = 117.5
JACK_NAMES = ["TRIG", "OPEN", "ACC", "V/OCT", "OUT"]
JACK_X = [RX0 + (RX1 - RX0) / 10 * (2 * k + 1) for k in range(5)]

# ---------------------------------------------------------------- drawing
fonts = {}


def text(s, x, y, size, font=FONT_MED, fill=INK, anchor="middle", spacing=0.04, rotate=None):
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    off = {"middle": -width * scale / 2, "end": -width * scale}.get(anchor, 0.0)
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        tp = TransformPen(pen, (scale, 0, 0, -scale, off + cx * scale, 0))
        gs[n].draw(tp)
        cx += gs[n].width + track
    tr = f"translate({x:.3f},{y:.3f})" + (f" rotate({rotate})" if rotate else "")
    return f'<path transform="{tr}" fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append


def rect(x, y, w, h, fill, r=1.2):
    add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{r}" fill="{fill}"/>')


def line(x1, y1, x2, y2, color=LINE, w=0.4):
    add(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{color}" stroke-width="{w}"/>')


def label(s, x, y, size=CTRL, fill=INK, font=FONT_MED, anchor="middle", rotate=None):
    add(text(s, x, y, size, font=font, fill=fill, anchor=anchor, rotate=rotate))


rect(0, 0, W, H, BG, r=0)
label("ALEXA01", 6.0, 9.6, 4.6, fill=INK, font=FONT_BOLD, anchor="start")
label("one LXR drum voice, six memories", 33.5, 9.4, 2.6, fill=SOFT, anchor="start")

# knob sections
for (x, y, w, h, title) in boxes:
    rect(x, y, w, h, BG2, r=1.6)
    rect(x, y, STRIP, h, "#383D45", r=1.6)
    label(title, x + STRIP / 2 + 1.15, y + h / 2, CTRL, fill=ACCENT, font=FONT_BOLD, rotate=-90)
for s, (x, y) in enumerate(slot_pos):
    label(SLOT_LABELS[s], x, y - 7.2, CTRL)

# screen frame (the screen itself is drawn by the module)
rect(DISP[0] - 0.8, DISP[1] - 0.8, DISP[2] + 1.6, DISP[3] + 1.6, OUTBOX, r=1.6)

for k, x in enumerate(TYPE_X):
    label(TYPE_NAMES[k], x, TYPE_Y - 4.9, CTRL)
label("MEMORIES", RCX, MEM_Y - 8.6, JACK, fill=SOFT, font=FONT_BOLD)
for k, x in enumerate(MEM_X):
    label(str(k + 1), x, MEM_Y - 4.6, CTRL)
for k, x in enumerate(ACT_X):
    label(ACT_NAMES[k], x, ACT_Y - 4.9, CTRL)
line(RX0, 85.0, RX1, 85.0)

label("MORPH", MORPH_KNOB[0], 89.4 - 1.4, CTRL, fill=ACCENT, font=FONT_BOLD)
label("MEM", MODE_SW[0], MODE_SW[1] - 5.6, CTRL)
label("SCAN", MODE_SW[0], MODE_SW[1] + 8.4, CTRL)
label("CV", MORPH_ATT[0] - 5.2, MORPH_ATT[1] + 1.1, CTRL, anchor="end")

rect(JACK_X[4] - 6.0, JACK_Y - 8.8, 12.0, 13.6, OUTBOX, r=1.4)
for k, x in enumerate(JACK_X):
    label(JACK_NAMES[k], x, JACK_Y - 5.6, JACK, fill=INK if k == 4 else SOFT)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
       + "".join(parts) + "</svg>\n")
out = os.path.join(ROOT, "res/Alexa01.svg")
open(out, "w").write(svg)

# Haer maker logo on the bottom rail
sys.path.insert(0, os.path.join(ROOT, "res"))
import haer_logo
haer_logo.add_to_file(out, x=LEFT + 30.0, y=126.0, cap=3.0)

# ---------------------------------------------------------------- C++ layout
def flist(v):
    return "{" + ", ".join(f"{x:.2f}f" for x in v) + "}"

hpp = ["// Generated by tools/make_panel_alexa01.py. Positions in mm. Do not edit.", "#pragma once", "",
       "namespace layout01 {",
       f"static const float SLOT_X[32] = {flist([p[0] for p in slot_pos])};",
       f"static const float SLOT_Y[32] = {flist([p[1] for p in slot_pos])};",
       f"static const float DISP_X = {DISP[0]:.2f}f, DISP_Y = {DISP[1]:.2f}f, DISP_W = {DISP[2]:.2f}f, DISP_H = {DISP[3]:.2f}f;",
       f"static const float TYPE_Y = {TYPE_Y}f;",
       f"static const float TYPE_X[4] = {flist(TYPE_X)};",
       f"static const float MEM_Y = {MEM_Y}f;",
       f"static const float MEM_X[6] = {flist(MEM_X)};",
       f"static const float ACT_Y = {ACT_Y}f;",
       f"static const float ACT_X[4] = {flist(ACT_X)};",
       f"static const float MORPH_KNOB_X = {MORPH_KNOB[0]:.2f}f, MORPH_KNOB_Y = {MORPH_KNOB[1]:.2f}f;",
       f"static const float MODE_X = {MODE_SW[0]:.2f}f, MODE_Y = {MODE_SW[1]:.2f}f;",
       f"static const float MORPH_ATT_X = {MORPH_ATT[0]:.2f}f, MORPH_ATT_Y = {MORPH_ATT[1]:.2f}f;",
       f"static const float MORPH_CV_X = {MORPH_CV[0]:.2f}f, MORPH_CV_Y = {MORPH_CV[1]:.2f}f;",
       f"static const float JACK_Y = {JACK_Y}f;",
       f"static const float JACK_X[5] = {flist(JACK_X)};",
       "} // namespace layout01", ""]
open(os.path.join(ROOT, "src/Layout01.hpp"), "w").write("\n".join(hpp))
print("wrote res/Alexa01.svg and src/Layout01.hpp")
