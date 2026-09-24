#!/usr/bin/env python3
# Stained Glass scroll bars for Wine's Light visual style (patch 0065).
#
#   theme/scrollbar.py DIR    rewrite DIR/blue_scrollbar_*.svg
#
# Our own drawing, in the Windows 10 manner: a flat light track, a flat
# borderless thumb that darkens when hot and pressed, small solid triangle
# arrows on buttons that take the track's colour, no gripper. The image grids
# (which cell is which state) are the ones the theme INI declares for Light:
#   arrows/glyphs: 20 cells -- up, down, left, right x (normal, hot, pressed,
#                  disabled), then the four directions' hover cells
#   thumb, tracks: 5 cells  -- normal, hot, pressed, disabled, hover
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import os
import sys

TRACK = "#f0f0f0"
# per state: normal, hot, pressed, disabled, hover
THUMB = ["#cdcdcd", "#a6a6a6", "#606060", TRACK, "#c2c2c2"]
BUTTON = [TRACK, "#dadada", "#606060", TRACK, TRACK]
GLYPH = ["#606060", "#000000", "#ffffff", "#bfbfbf", "#606060"]

HEAD = ('<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg id="bitmap:%d-24" width="%d" height="%d" version="1.1" viewBox="0 0 %d %d" '
        'xmlns="http://www.w3.org/2000/svg" shape-rendering="%s">\n')


def svg(w, h, body, crisp=True):
    return HEAD % (w, w, h, w, h, "crispEdges" if crisp else "geometricPrecision") + body + "</svg>\n"


def cells():
    """(direction, state) for each of the 20 arrow cells, in order."""
    out = [(d, s) for d in range(4) for s in range(4)]
    return out + [(d, 4) for d in range(4)]


def triangle(d, x, y, n):
    """Points of a solid triangle pointing up/down/left/right in an n x n cell."""
    w = max(5, round(n * 0.47)) | 1          # odd, so it has a centre column
    h = (w + 1) // 2
    cx, cy = x + n / 2, y + n / 2
    if d in (0, 1):
        top, bot = cy - h / 2, cy + h / 2
        pts = [(cx - w / 2, bot), (cx + w / 2, bot), (cx, top)] if d == 0 else \
              [(cx - w / 2, top), (cx + w / 2, top), (cx, bot)]
    else:
        l, r = cx - h / 2, cx + h / 2
        pts = [(r, cy - w / 2), (r, cy + w / 2), (l, cy)] if d == 2 else \
              [(l, cy - w / 2), (l, cy + w / 2), (r, cy)]
    return " ".join("%.2f,%.2f" % p for p in pts)


def arrows():
    n = 17
    body = "".join('<rect x="0" y="%d" width="%d" height="%d" fill="%s"/>\n' % (i * n, n, n, BUTTON[s])
                   for i, (d, s) in enumerate(cells()))
    return svg(n, 20 * n, body)


def glyphs(n):
    body = "".join('<polygon points="%s" fill="%s"/>\n' % (triangle(d, 0, i * n, n), GLYPH[s])
                   for i, (d, s) in enumerate(cells()))
    return svg(n, 20 * n, body, crisp=False)


def thumb(vertical):
    # A cell per state; the flat bar inset by 2 px from the long edges, on
    # the track, so it reads as a bar in a channel.
    w, h = (17, 11) if vertical else (20, 17)
    body = ""
    for i, c in enumerate(THUMB):
        oy = i * h
        body += '<rect x="0" y="%d" width="%d" height="%d" fill="%s"/>\n' % (oy, w, h, TRACK)
        if vertical:
            body += '<rect x="2" y="%d" width="%d" height="%d" fill="%s"/>\n' % (oy, w - 4, h, c)
        else:
            body += '<rect x="0" y="%d" width="%d" height="%d" fill="%s"/>\n' % (oy + 2, w, h - 4, c)
    return svg(w, 5 * h, body)


def track(w, h):
    body = "".join('<rect x="0" y="%d" width="%d" height="1" fill="%s"/>\n' % (i, w, TRACK) for i in range(h))
    return svg(w, h, body)


def main(d):
    files = {"blue_scrollbar_arrows.svg": arrows(),
             "blue_scrollbar_thumb_vertical.svg": thumb(True),
             "blue_scrollbar_thumb_horizontal.svg": thumb(False)}
    for n in (13, 16, 20, 26, 32, 39, 52):
        files["blue_scrollbar_arrow_glyphs_%dpx.svg" % n] = glyphs(n)
    for name in ("upper", "lower"):
        files["blue_scrollbar_%s_track_vertical.svg" % name] = track(14, 5)
        files["blue_scrollbar_%s_track_horizontal.svg" % name] = track(1, 40)
    for name, text in files.items():
        with open(os.path.join(d, name), "w") as f:
            f.write(text)


if __name__ == "__main__":
    main(sys.argv[1])
