#!/usr/bin/env python3
# The Stained Glass Dark colour scheme of Wine's Light visual style (patch 0160).
#
#   theme/dark.py DIR     DIR = dlls/light.msstyles of a patched tree
#
# light.msstyles carries two colour schemes, "Blue" (Stained Glass Light,
# patches 0031/0065/0066) and "Dark". Dark is *derived* from Light rather
# than drawn a second time, so every control the Light style knows has a dark
# look and the two never drift apart:
#
#   dark_*.bmp  every blue_*.bmp with its pixels remapped (below)
#   dark.rc     DARK_INI -- BLUE_INI with its images renamed, its colours
#               remapped, and the system colours replaced by DARK_SYS --
#               and the BITMAP resources for dark_*.bmp. light.rc
#               #includes it.
#
# The remapping: neutral colours (greys, whites) have their lightness turned
# over onto a dark ramp -- white surfaces become #333, light borders mid
# grey, black glyphs near white. Pale tints of the accent (hover and selection
# backgrounds) become dark tints of it. The accent itself stays: purple reads
# on dark as it does on light.
#
# build.sh runs this after the series is applied and the Light images are
# rendered, so the scheme follows every later change to Light by itself.
# Our own work, from the rendered Light images; nothing is drawn by hand.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import colorsys
import os
import re
import struct
import sys

# The dark system colours, Windows 10 dark mode's shape in the Stained Glass
# palette. Names are the theme INI's (TMT_*), not the registry's.
DARK_SYS = [
    ("Scrollbar", (23, 23, 23)),
    ("ActiveCaption", (32, 32, 32)),
    ("InactiveCaption", (43, 43, 43)),
    ("Menu", (43, 43, 43)),
    ("Window", (32, 32, 32)),
    ("WindowFrame", (70, 70, 70)),
    ("MenuText", (255, 255, 255)),
    ("WindowText", (255, 255, 255)),
    ("CaptionText", (255, 255, 255)),
    ("ActiveBorder", (32, 32, 32)),
    ("InactiveBorder", (43, 43, 43)),
    ("AppWorkSpace", (50, 50, 50)),
    ("Highlight", (123, 47, 190)),
    ("HighlightText", (255, 255, 255)),
    ("BtnFace", (43, 43, 43)),
    ("BtnShadow", (85, 85, 85)),
    ("GrayText", (130, 130, 130)),
    ("BtnText", (255, 255, 255)),
    ("InactiveCaptionText", (170, 170, 170)),
    ("BtnHighlight", (70, 70, 70)),
    ("DkShadow3d", (20, 20, 20)),
    ("Light3d", (60, 60, 60)),
    ("InfoText", (255, 255, 255)),
    ("InfoBk", (43, 43, 43)),
    ("ButtonAlternateFace", (43, 43, 43)),
    ("HotTracking", (190, 150, 240)),
    ("GradientActiveCaption", (32, 32, 32)),
    ("GradientInactiveCaption", (43, 43, 43)),
    ("MenuHilight", (123, 47, 190)),
    ("MenuBar", (32, 32, 32)),
]
SYS_NAMES = {n.lower() for n, _ in DARK_SYS}

# What Light leaves to the program's DC -- black text, which on the dark
# window background would vanish -- the Dark scheme says itself.
EXTRA = [
    ("Button.Checkbox", "TextColor = 255 255 255"),
    ("Button.Radiobutton", "TextColor = 255 255 255"),
] + [("Button.Checkbox(%sDisabled)" % st, "TextColor = 130 130 130")
     for st in ("Unchecked", "Checked", "Mixed", "Implicit", "Excluded")] \
  + [("Button.Radiobutton(%sDisabled)" % st, "TextColor = 130 130 130")
     for st in ("Unchecked", "Checked")]

# the dark ramp for neutral colours: lightness 1.0 -> LO, 0.0 -> HI
LO, HI = 0.20, 0.92


def remap(r, g, b):
    """Map one Light colour to its Dark counterpart (0..255 ints)."""
    h, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
    if s < 0.20 or l > 0.97 or l < 0.03:
        # a grey: turn its lightness over onto the dark ramp, keep a hint of hue
        l2 = LO + (1.0 - l) * (HI - LO)
        s2 = min(s, 0.10)
    elif l > 0.72:
        # a pale tint (hover/selection backgrounds): a dark tint of the same hue
        l2 = LO + (1.0 - l) * 0.9
        s2 = min(0.45, s)
    else:
        # the accent itself, and deep shades of it: keep
        return r, g, b
    rr, gg, bb = colorsys.hls_to_rgb(h, l2, s2)
    return int(round(rr * 255)), int(round(gg * 255)), int(round(bb * 255))


def remap_bmp(src, dst):
    """Remap every pixel of a 32-bit or 24-bit bitmap (the rendered images are
    32-bit with alpha; some of the tarball's pre-rendered ones are 24-bit)."""
    data = bytearray(open(src, "rb").read())
    if data[:2] != b"BM":
        raise ValueError(src + ": not a bitmap")
    off = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise ValueError(src + ": %d bpp, expected 24 or 32" % bpp)
    step = bpp // 8
    stride = (width * step + 3) & ~3
    cache = {}
    for row in range(abs(height)):
        base = off + row * stride
        for i in range(base, base + width * step, step):
            if step == 4 and data[i + 3] == 0:
                continue
            key = (data[i + 2], data[i + 1], data[i])
            out = cache.get(key)
            if out is None:
                out = cache[key] = remap(*key)
            data[i + 2], data[i + 1], data[i] = out
    with open(dst, "wb") as f:
        f.write(data)


def ini_block(rc, name):
    """The lines of a TEXTFILE resource's body in light.rc."""
    m = re.search(r"^%s TEXTFILE\s*\n\{\n(.*?)^\}" % name, rc, re.S | re.M)
    if not m:
        raise ValueError("no %s in light.rc" % name)
    return m.group(1).splitlines()


RGB_LINE = re.compile(r'^("\s*)([A-Za-z0-9]+)(\s*=\s*)(\d+)\s+(\d+)\s+(\d+)(\\r\\n"\s*)$')
IMAGE_REF = re.compile(r"blue_([a-z0-9_]+)\.bmp", re.I)


def dark_ini(lines):
    out, section = [], ""
    for line in lines:
        sec = re.match(r'^"(?:\\r\\n)*\[([^\]]*)\]', line)
        if sec:
            section = sec.group(1).lower()
        m = RGB_LINE.match(line)
        if m:
            name = m.group(2)
            r, g, b = int(m.group(4)), int(m.group(5)), int(m.group(6))
            if section == "sysmetrics" and name.lower() == "background":
                pass    # the desktop's colour is the wallpaper's business
            elif section == "sysmetrics" and name.lower() in SYS_NAMES:
                r, g, b = dict((n.lower(), v) for n, v in DARK_SYS)[name.lower()]
            elif name.lower().endswith("textcolor") and min(r, g, b) >= 230:
                pass    # white text on the accent stays white
            elif name.lower().endswith("textcolor") and max(r, g, b) < 40:
                r, g, b = 255, 255, 255     # black text reads as white, as on Windows
            else:
                r, g, b = remap(r, g, b)
            line = "%s%s%s%d %d %d%s" % (m.group(1), name, m.group(3), r, g, b, m.group(7))
        line = IMAGE_REF.sub(lambda mm: "dark_%s.bmp" % mm.group(1), line)
        out.append(line)
    out.append('"\\r\\n; Stained Glass Dark: text Light leaves to the program\\r\\n"')
    for sec, prop in EXTRA:
        out.append('"[%s]\\r\\n"' % sec)
        out.append('"%s\\r\\n"' % prop)
    return out


def main(d):
    rc = open(os.path.join(d, "light.rc"), encoding="utf-8").read()
    body = dark_ini(ini_block(rc, "BLUE_INI"))
    images = sorted(f for f in os.listdir(d) if f.startswith("blue_") and f.endswith(".bmp"))
    res = ["/* Generated by wine-sg theme/dark.py from light.rc and the rendered",
           " * Light images -- do not edit; change the script. */", "",
           "/* Dark theme */", "DARK_INI TEXTFILE", "{"] + body + ["}", ""]
    for f in images:
        dark = "dark_" + f[5:]
        remap_bmp(os.path.join(d, f), os.path.join(d, dark))
        res += ["/* @makedep: %s */" % dark,
                '%s BITMAP "%s"' % (dark.upper().replace(".", "_"), dark), ""]
    with open(os.path.join(d, "dark.rc"), "w", encoding="utf-8") as f:
        f.write("\n".join(res))
    print("dark.py: %d images, %d INI lines" % (len(images), len(body)))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: dark.py DIR (dlls/light.msstyles)")
    main(sys.argv[1])
