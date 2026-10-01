#!/usr/bin/env python3
# The Rounded and Rounded Dark colour schemes of Wine's Light visual style
# (patch 0486): the controls of the Rounded style (Settings > Personalization
# > Colors > Window style), in the manner of newer Windows -- rounder corners,
# a blue accent, lighter neutral edges.
#
#   theme/rounded.py DIR     DIR = dlls/light.msstyles of a patched tree,
#                            after its Light images are rendered
#
# Like theme/dark.py, the schemes are *derived* from Light, so every control
# Light knows has a Rounded look and they never drift apart:
#
#   rounded_*.svg/.bmp   every blue_*.svg with its rectangles' corner radii
#                        doubled (a quarter more for check boxes, which stay
#                        square; square frames get round corners),
#                        the purple accent turned to blue and the mid greys
#                        (edges) lightened, rendered by tools/buildimage
#   roundeddark_*.bmp    those, remapped onto the dark ramp by dark.py's rule
#   rounded.rc           ROUNDED_INI and ROUNDEDDARK_INI -- BLUE_INI with its
#                        images renamed and its colours moved the same way,
#                        its bordered fills round (BorderType = RoundRect,
#                        0740) and a focused edit box's frame the accent --
#                        and the BITMAP resources. light.rc #includes it.
#
# build.sh runs this after dark.py. Our own work; nothing is drawn by hand.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import colorsys
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dark  # noqa: E402  (the dark ramp, and the INI and bitmap helpers)

ACCENT_HUE = 207.0 / 360.0    # newer Windows' default accent, #005FB8, is this blue
LIGHT_ACCENT = (123, 47, 190)  # Light's purple accent (a default button's frame)


def recolour(r, g, b):
    """A Light colour in the Rounded scheme (0..255 ints)."""
    h, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
    if s >= 0.25 and 0.65 <= h <= 0.92:
        # the purple accent and its shades: the same shade of blue
        rr, gg, bb = colorsys.hls_to_rgb(ACCENT_HUE, l, min(1.0, s * 1.15))
    elif s < 0.20 and 0.45 <= l <= 0.80:
        # a mid grey -- an edge: lighter, as newer Windows draws them
        l2 = l + (1.0 - l) * 0.35
        rr, gg, bb = colorsys.hls_to_rgb(h, l2, s)
    else:
        return r, g, b
    return int(round(rr * 255)), int(round(gg * 255)), int(round(bb * 255))


HEX = re.compile(r"#([0-9a-fA-F]{6})\b|#([0-9a-fA-F]{3})\b")
RADIUS = re.compile(r'\b(rx|ry)="([0-9.]+)"')
RECT = re.compile(r"<rect\b[^>]*>")
# Only a rectangle's corner radii: an <ellipse>'s rx and ry are its size
# (doubling them drew radio buttons as broken arcs).

# The parts drawn as a bordered fill (BgType = BorderFill) whose corners are
# round in the Rounded scheme, by uxtheme's BorderType = RoundRect (0740):
# the frames of edit boxes, combo boxes, list boxes and list and tree views
# (Explorer's too).
ROUND_FILLS = ("Edit", "ComboBox", "ListBox", "ListView", "TreeView", "Explorer::ListView", "Explorer::TreeView")
ROUND_FILL_PROPS = ["BorderType = RoundRect", "RoundCornerWidth = 8", "RoundCornerHeight = 8"]
# Frames drawn from square images: their rectangles get the radius the
# 2-pixel sizing margins hold (an edit box's EditBorder parts, a combo box's
# border).
SQUARE_FRAMES = ("edit_border_", "combobox_border")
FRAME_RX = ".45"


def rounded_svg(text, factor=2.0):
    def hexsub(m):
        h = m.group(1) or "".join(c * 2 for c in m.group(2))
        r, g, b = recolour(int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))
        return "#%02x%02x%02x" % (r, g, b)

    def radsub(m):
        return '%s="%s"' % (m.group(1), "%.5g" % (float(m.group(2)) * factor))

    text = HEX.sub(hexsub, text)
    return RECT.sub(lambda m: RADIUS.sub(radsub, m.group(0)), text)


def round_frame_svg(text):
    """A square frame's rectangles given round corners."""
    def add(m):
        r = m.group(0)
        if "rx=" in r:
            return r
        return r[:5] + ' rx="%s" ry="%s"' % (FRAME_RX, FRAME_RX) + r[5:]
    return RECT.sub(add, text)


def round_fills(lines):
    """The ROUND_FILLS sections, with round corners."""
    out = []
    for line in lines:
        out.append(line)
        m = re.match(r'^"\[([A-Za-z:]+)\]\\r\\n"$', line.strip())
        if m and m.group(1) in ROUND_FILLS:
            out += ['"%s\\r\\n"' % p for p in ROUND_FILL_PROPS]
    # a focused edit box's frame in the accent colour, as newer Windows marks it
    out += ['', '"[Edit.EditText(Focused)]\\r\\n"', '"BorderColor = %d %d %d\\r\\n"' % recolour(*LIGHT_ACCENT)]
    return out


def render(tree, svg, bmp):
    env = dict(os.environ, CONVERT="convert", ICOTOOL="icotool", RSVG="rsvg-convert")
    r = subprocess.run(["perl", os.path.join(tree, "tools", "buildimage"), svg, bmp], env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        # say why: a quiet failure here stopped CI's image build with no reason
        sys.stderr.write("rounded.py: buildimage %s -> %s failed (%d):\n%s\n" % (svg, bmp, r.returncode, r.stdout))
        raise SystemExit(1)


def rounded_ini(lines, prefix):
    """BLUE_INI's lines for the Rounded scheme; prefix names its images."""
    out = []
    for line in lines:
        m = dark.RGB_LINE.match(line)
        if m:
            r, g, b = recolour(int(m.group(4)), int(m.group(5)), int(m.group(6)))
            line = "%s%s%s%d %d %d%s" % (m.group(1), m.group(2), m.group(3), r, g, b, m.group(7))
        out.append(dark.IMAGE_REF.sub(lambda mm: "%s_%s.bmp" % (prefix, mm.group(1)), line))
    return out


def main(d):
    tree = os.path.normpath(os.path.join(d, "..", ".."))
    rc = open(os.path.join(d, "light.rc"), encoding="utf-8").read()
    blue = dark.ini_block(rc, "BLUE_INI")
    light_body = round_fills(rounded_ini(blue, "rounded"))
    # Rounded Dark: the Rounded INI through dark.py's remapping, its images renamed
    dark_body = [line.replace("roundeddark_", "rounded_") for line in dark.dark_ini(light_body)]
    dark_body = [re.sub(r"rounded_([a-z0-9_]+)\.bmp", r"roundeddark_\1.bmp", line) for line in dark_body]

    images = sorted(f for f in os.listdir(d) if f.startswith("blue_") and f.endswith(".bmp"))
    res = ["/* Generated by wine-sg theme/rounded.py from light.rc and the Light",
           " * images -- do not edit; change the script. */", "",
           "/* Rounded theme */", "ROUNDED_INI TEXTFILE", "{"] + light_body + ["}", "",
           "/* Rounded Dark theme */", "ROUNDEDDARK_INI TEXTFILE", "{"] + dark_body + ["}", ""]
    for f in images:
        stem = f[5:-4]
        svg = os.path.join(d, "blue_%s.svg" % stem)
        out_svg = os.path.join(d, "rounded_%s.svg" % stem)
        out_bmp = os.path.join(d, "rounded_%s.bmp" % stem)
        if os.path.exists(svg):
            with open(svg, encoding="utf-8") as fh:
                text = fh.read()
            with open(out_svg, "w", encoding="utf-8") as fh:
                # a check box's small square stays a square with round corners
                text = rounded_svg(text, 1.25 if stem.startswith("checkbox") else 2.0)
                if stem.startswith(SQUARE_FRAMES):
                    text = round_frame_svg(text)
                fh.write(text)
            render(tree, out_svg, out_bmp)
        else:
            # a pre-rendered image without a source: its colours, moved the same way
            remap_bitmap(os.path.join(d, f), out_bmp)
        dark.remap_bmp(out_bmp, os.path.join(d, "roundeddark_%s.bmp" % stem))
        for prefix in ("rounded", "roundeddark"):
            name = "%s_%s.bmp" % (prefix, stem)
            res += ["/* @makedep: %s */" % name, '%s BITMAP "%s"' % (name.upper().replace(".", "_"), name), ""]
    with open(os.path.join(d, "rounded.rc"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(res))
    print("rounded.py: %d images, %d INI lines" % (len(images), len(light_body)))


def remap_bitmap(src, dst):
    """recolour() every pixel of a 24- or 32-bit bitmap."""
    saved = dark.remap
    dark.remap = recolour
    try:
        dark.remap_bmp(src, dst)
    finally:
        dark.remap = saved


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: rounded.py DIR (dlls/light.msstyles)")
    main(sys.argv[1])
