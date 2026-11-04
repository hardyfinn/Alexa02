#!/usr/bin/env python3
"""Draws res/Alexa02.svg and writes src/Layout.hpp with the same positions.

VCV Rack can't draw SVG <text>, so every label is turned into outline paths
with fontTools. All positions are in mm.
    pip install fonttools
    python3 tools/make_panel.py
"""
import os
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
VOICE_COLS = ["#E86A4A", "#E8A04A", "#D9C64A", "#7BC47F", "#57B5C9", "#9C8CE0"]

# ---------------------------------------------------------------- layout
DISP = (6.0, 11.0, 108.0, 34.0)                       # x, y, w, h
EDIT_Y = 54.0
EDIT_X = [DISP[0] + DISP[2] / 12.0 * (2 * k + 1) for k in range(6)]

VOICE_X = [22.0, 36.0, 50.0, 64.0, 78.0, 92.0]
OH_X = 106.0
ROW_SEL, ROW_TRIG, ROW_ACC, ROW_VOCT, ROW_MORPH, ROW_OUT = 67.0, 78.5, 89.0, 99.5, 110.0, 120.5

PAGE_X = [131.0, 148.0, 165.0, 182.0, 199.0]
PAGE_Y = [19.0, 34.0]
PAGE_NAMES = ["OSC", "AMP", "PITCH", "FM", "CLICK", "FILTER", "LFO", "MIX", "FX", "MOD"]

KIT_Y = 52.5
EDITAB_X, RND_X, COPY_X, PLAY_X = 131.0, 148.0, 165.0, 182.0

CV_X = 128.0
MORPH_KNOB = (153.0, 90.0)
MORPH_ATT = (176.0, 84.0)
MORPH_CV = (176.0, 97.5)
OUT_L = (199.0, ROW_MORPH)
OUT_R = (199.0, ROW_OUT)

VOICE_NAMES = ["DRUM 1", "DRUM 2", "DRUM 3", "SNARE", "CYMBAL", "HAT"]

# ---------------------------------------------------------------- drawing
fonts = {}


def text(s, x, y, size, font=FONT_MED, fill=INK, anchor="middle", spacing=0.04):
    """SVG path for string s; (x, y) is the baseline point in mm."""
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    if anchor == "middle":
        x0 = x - width * scale / 2
    elif anchor == "end":
        x0 = x - width * scale
    else:
        x0 = x
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        tp = TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y))
        gs[n].draw(tp)
        cx += gs[n].width + track
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append


def rect(x, y, w, h, fill, r=1.2, stroke=None, sw=0.3):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{r}" fill="{fill}"{s}/>')


def line(x1, y1, x2, y2, color=SOFT, w=0.3):
    add(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{color}" stroke-width="{w}"/>')


def label(s, x, y, size=2.3, fill=SOFT, font=FONT_MED, anchor="middle"):
    add(text(s, x, y, size, font=font, fill=fill, anchor=anchor))


rect(0, 0, W, H, BG, r=0)

# title
label("ALEXA02", 6.0, 7.4, 4.6, fill=INK, font=FONT_BOLD, anchor="start")
label("six drum voices from the open LXR engine", 33.5, 7.2, 2.2, fill=SOFT, anchor="start")

# display frame (the display itself is drawn by the module)
rect(DISP[0] - 0.8, DISP[1] - 0.8, DISP[2] + 1.6, DISP[3] + 1.6, OUTBOX, r=1.6)

# edit knobs: small numbers under them
for k, x in enumerate(EDIT_X):
    label(str(k + 1), x, EDIT_Y + 7.6, 2.0)

# voice columns
rect(15.0, 59.5, 98.0, 3.2, BG, r=0)
for v, x in enumerate(VOICE_X):
    c = VOICE_COLS[v]
    rect(x - 6.2, 59.0, 12.4, 67.0, BG2, r=1.6)
    add(f'<rect x="{x-6.2:.2f}" y="59.0" width="12.4" height="1.1" rx="0.5" fill="{c}"/>')
    label(VOICE_NAMES[v], x, 62.6, 2.2, fill=INK, font=FONT_BOLD)
rect(OH_X - 6.2, ROW_TRIG - 6.6, 12.4, 13.0, BG2, r=1.6)
label("OPEN", OH_X, ROW_TRIG - 4.4, 2.0, fill=INK, font=FONT_BOLD)

# row labels
for name, y in [("SELECT", ROW_SEL), ("TRIG", ROW_TRIG), ("ACC", ROW_ACC), ("V/OCT", ROW_VOCT),
                ("MORPH", ROW_MORPH), ("OUT", ROW_OUT)]:
    label(name, 14.6, y + 0.8, 2.0, anchor="end")
label("CLOSED", VOICE_X[5], ROW_TRIG - 4.4, 1.8, fill=SOFT)

# individual outputs box
rect(15.6, ROW_OUT - 4.9, 82.8, 9.8, OUTBOX, r=1.4)

# right section: pages
label("PAGE", 116.5, 13.0, 2.2, fill=INK, font=FONT_BOLD, anchor="start")
label("press again for the next part of a page", 210.0, 13.0, 1.8, anchor="end")
for i, name in enumerate(PAGE_NAMES):
    x = PAGE_X[i % 5]
    y = PAGE_Y[i // 5]
    label(name, x, y - 4.6, 2.1, fill=INK)
line(117, 41.5, 210, 41.5, color="#3A3F47", w=0.4)

# kit buttons
for name, x in [("EDIT", EDITAB_X), ("MUTATE", RND_X), ("A>B", COPY_X), ("PLAY", PLAY_X)]:
    label(name, x, KIT_Y - 5.0, 2.1, fill=INK)
label("A", EDITAB_X - 4.6, KIT_Y + 6.3, 1.9, fill=SOFT)
label("B", EDITAB_X + 4.6, KIT_Y + 6.3, 1.9, fill=SOFT)
label("kit B from A", RND_X, KIT_Y + 6.3, 1.7)
label("this voice", COPY_X, KIT_Y + 6.3, 1.7)
label("this voice", PLAY_X, KIT_Y + 6.3, 1.7)
line(117, 63.0, 210, 63.0, color="#3A3F47", w=0.4)

# mod CV column
label("MOD CV", CV_X, ROW_TRIG - 6.2, 2.1, fill=INK, font=FONT_BOLD)
for i, y in enumerate([ROW_TRIG, ROW_ACC, ROW_VOCT, ROW_MORPH, ROW_OUT]):
    label(str(i + 1), CV_X - 5.6, y + 0.8, 2.0, anchor="end")

# morph
label("MORPH", MORPH_KNOB[0], 74.0, 3.2, fill=ACCENT, font=FONT_BOLD)
label("A", MORPH_KNOB[0] - 9.5, MORPH_KNOB[1] + 9.2, 2.6, fill=INK, font=FONT_BOLD)
label("B", MORPH_KNOB[0] + 9.5, MORPH_KNOB[1] + 9.2, 2.6, fill=INK, font=FONT_BOLD)
label("CV", MORPH_ATT[0], MORPH_ATT[1] - 4.6, 2.0)
label("each voice adds its MORPH jack", MORPH_KNOB[0] + 3, 108.0, 1.7, anchor="middle")

# mix outs
rect(OUT_L[0] - 6.0, OUT_L[1] - 9.4, 12.0, 25.5, OUTBOX, r=1.4)
label("MIX", OUT_L[0], OUT_L[1] - 5.6, 2.2, fill=INK, font=FONT_BOLD)
label("L", OUT_L[0] - 4.9, OUT_L[1] + 0.8, 1.9, anchor="end")
label("R", OUT_R[0] - 4.9, OUT_R[1] + 0.8, 1.9, anchor="end")

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
       + "".join(parts) + "</svg>\n")
open(os.path.join(ROOT, "res/Alexa02.svg"), "w").write(svg)

# ---------------------------------------------------------------- C++ layout
def v(p):
    return f"Vec({p[0]:.2f}f, {p[1]:.2f}f)"

hpp = ["// Generated by tools/make_panel.py. Positions in mm. Do not edit.", "#pragma once", "",
       "namespace layout {",
       f"static const float DISP_X = {DISP[0]}f, DISP_Y = {DISP[1]}f, DISP_W = {DISP[2]}f, DISP_H = {DISP[3]}f;",
       f"static const float EDIT_Y = {EDIT_Y}f;",
       "static const float EDIT_X[6] = {" + ", ".join(f"{x:.2f}f" for x in EDIT_X) + "};",
       "static const float VOICE_X[6] = {" + ", ".join(f"{x:.2f}f" for x in VOICE_X) + "};",
       f"static const float OH_X = {OH_X}f;",
       f"static const float ROW_SEL = {ROW_SEL}f, ROW_TRIG = {ROW_TRIG}f, ROW_ACC = {ROW_ACC}f, ROW_VOCT = {ROW_VOCT}f, ROW_MORPH = {ROW_MORPH}f, ROW_OUT = {ROW_OUT}f;",
       "static const float PAGE_X[5] = {" + ", ".join(f"{x:.2f}f" for x in PAGE_X) + "};",
       "static const float PAGE_Y[2] = {" + ", ".join(f"{x:.2f}f" for x in PAGE_Y) + "};",
       f"static const float KIT_Y = {KIT_Y}f, EDITAB_X = {EDITAB_X}f, RND_X = {RND_X}f, COPY_X = {COPY_X}f, PLAY_X = {PLAY_X}f;",
       f"static const float CV_X = {CV_X}f;",
       f"static const float MORPH_KNOB_X = {MORPH_KNOB[0]}f, MORPH_KNOB_Y = {MORPH_KNOB[1]}f;",
       f"static const float MORPH_ATT_X = {MORPH_ATT[0]}f, MORPH_ATT_Y = {MORPH_ATT[1]}f;",
       f"static const float MORPH_CV_X = {MORPH_CV[0]}f, MORPH_CV_Y = {MORPH_CV[1]}f;",
       f"static const float OUT_L_X = {OUT_L[0]}f, OUT_L_Y = {OUT_L[1]}f, OUT_R_X = {OUT_R[0]}f, OUT_R_Y = {OUT_R[1]}f;",
       "} // namespace layout", ""]
open(os.path.join(ROOT, "src/Layout.hpp"), "w").write("\n".join(hpp))
print("wrote res/Alexa02.svg and src/Layout.hpp")
