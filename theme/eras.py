#!/usr/bin/env python3
# The Horizon and Glass colour schemes of Wine's Light visual style (patch
# 0743): the controls of the Horizon and Glass looks (Settings >
# Personalization), in the manner of the 2001-era and 2009-era desktops --
# Horizon's rounded beige buttons with a dark blue edge and an orange glow
# when hot, green check marks and radio dots, lavender-blue scroll bars,
# green progress blocks, white menus with a blue highlight; Glass's glossy
# two-tone grey buttons that turn light blue under the pointer, blue radio
# dots, glossy grey scroll bars and a glossy green progress bar.
#
#   theme/eras.py DIR        DIR = dlls/light.msstyles of a patched tree,
#                            after its Light images are rendered
#
# As theme/rounded.py, the schemes are *derived* from Light, so every part
# Light knows has an era look:
#
#   horizon_*.svg/.bmp,      the main controls' images drawn here from
#   glass_*.svg/.bmp         scratch (push buttons, check boxes, radio
#                            buttons, scroll bars, group boxes) at the size
#                            and state count of Light's; every other image
#                            Light's with its colours moved to the era's
#   eras.rc                  HORIZON_INI and GLASS_INI -- BLUE_INI with the
#                            era's system colours, its images renamed, its
#                            colours moved, a few properties of the era's
#                            own -- and the BITMAP resources. light.rc
#                            #includes it.
#
# build.sh runs this after rounded.py. Our own work: nothing is copied or
# traced; the shapes and colours are written out below.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import colorsys
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dark  # noqa: E402  (the INI and bitmap helpers)
import rounded  # noqa: E402  (render())


def hexc(c):
    return "#%02x%02x%02x" % c


def parse(h):
    h = h.lstrip("#")
    if len(h) == 3:
        h = "".join(ch * 2 for ch in h)
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def shade(c, f):
    """c lighter (f > 0, towards white) or darker (f < 0, towards black)."""
    if f >= 0:
        return tuple(int(round(v + (255 - v) * f)) for v in c)
    return tuple(int(round(v * (1 + f))) for v in c)


# ---- the eras ------------------------------------------------------------------

ERAS = {
    "horizon": {
        "accent_hue": 217.0 / 360.0,
        # the system colours: a beige face, white windows and menus, a blue
        # selection, pale yellow tooltips, the blue title bars
        "sys": {
            "Scrollbar": (212, 208, 200), "ActiveCaption": (0, 84, 227), "InactiveCaption": (122, 150, 223),
            "Menu": (255, 255, 255), "Window": (255, 255, 255), "WindowFrame": (0, 0, 0),
            "MenuText": (0, 0, 0), "WindowText": (0, 0, 0), "CaptionText": (255, 255, 255),
            "ActiveBorder": (212, 208, 200), "InactiveBorder": (212, 208, 200), "AppWorkSpace": (128, 128, 128),
            "Highlight": (49, 106, 197), "HighlightText": (255, 255, 255), "BtnFace": (236, 233, 216),
            "BtnShadow": (172, 168, 153), "GrayText": (172, 168, 153), "BtnText": (0, 0, 0),
            "InactiveCaptionText": (216, 228, 248), "BtnHighlight": (255, 255, 255), "DkShadow3d": (113, 111, 100),
            "Light3d": (241, 239, 226), "InfoText": (0, 0, 0), "InfoBk": (255, 255, 225),
            "ButtonAlternateFace": (181, 181, 181), "HotTracking": (0, 0, 128),
            "GradientActiveCaption": (61, 149, 255), "GradientInactiveCaption": (157, 185, 235),
            "MenuHilight": (49, 106, 197), "MenuBar": (236, 233, 216),
        },
        "edge": (127, 157, 185),          # edit boxes' and lists' frames
        "face": (236, 233, 216),
        "group_text": (0, 70, 213),
        "group_edge": (208, 208, 191),
        "progress": {"chunk": 8, "space": 2, "fill": ((144, 238, 136), (40, 184, 40)),
                     "bar": (255, 255, 255), "bar_edge": (104, 104, 104)},
        "glyph": {"#606060": "#4d6185", "#000000": "#1e3a78", "#ffffff": "#ffffff", "#bfbfbf": "#c9c7ba"},
    },
    "glass": {
        "accent_hue": 207.0 / 360.0,
        "sys": {
            "Scrollbar": (200, 200, 200), "ActiveCaption": (153, 180, 209), "InactiveCaption": (191, 205, 219),
            "Menu": (240, 240, 240), "Window": (255, 255, 255), "WindowFrame": (100, 100, 100),
            "MenuText": (0, 0, 0), "WindowText": (0, 0, 0), "CaptionText": (0, 0, 0),
            "ActiveBorder": (180, 180, 180), "InactiveBorder": (244, 247, 252), "AppWorkSpace": (171, 171, 171),
            "Highlight": (51, 153, 255), "HighlightText": (255, 255, 255), "BtnFace": (240, 240, 240),
            "BtnShadow": (160, 160, 160), "GrayText": (109, 109, 109), "BtnText": (0, 0, 0),
            "InactiveCaptionText": (67, 78, 84), "BtnHighlight": (255, 255, 255), "DkShadow3d": (105, 105, 105),
            "Light3d": (227, 227, 227), "InfoText": (0, 0, 0), "InfoBk": (255, 255, 255),
            "ButtonAlternateFace": (0, 0, 0), "HotTracking": (0, 102, 204),
            "GradientActiveCaption": (185, 209, 234), "GradientInactiveCaption": (215, 228, 242),
            "MenuHilight": (51, 153, 255), "MenuBar": (240, 240, 240),
        },
        "edge": (171, 173, 179),
        "face": (240, 240, 240),
        "group_text": (30, 57, 91),
        "group_edge": (213, 223, 229),
        "progress": {"chunk": 1, "space": 0, "fill": ((132, 230, 140), (6, 166, 34)),
                     "bar": (230, 230, 230), "bar_edge": (178, 178, 178)},
        "glyph": {"#606060": "#4d4d4d", "#000000": "#000000", "#ffffff": "#202020", "#bfbfbf": "#a8a8a8"},
    },
}


def recolour_for(era):
    """A Light colour in the era's scheme: the purple accent becomes the era's
    blue; the grey edges its edge colour's hue; Horizon's pale greys its beige."""
    e = ERAS[era]
    eh, el, es = colorsys.rgb_to_hls(*(v / 255.0 for v in e["edge"]))

    def recolour(r, g, b):
        h, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
        if s >= 0.25 and 0.65 <= h <= 0.92:
            rr, gg, bb = colorsys.hls_to_rgb(e["accent_hue"], l, min(1.0, s * 1.05))
        elif s < 0.20 and 0.55 <= l <= 0.80:
            rr, gg, bb = colorsys.hls_to_rgb(eh, min(0.9, l * el / 0.68), es)   # an edge
        elif era == "horizon" and s < 0.20 and 0.90 <= l < 0.995:
            rr, gg, bb = colorsys.hls_to_rgb(52.0 / 360.0, l - 0.02, 0.25)      # a pale face: beige
        else:
            return r, g, b
        return int(round(rr * 255)), int(round(gg * 255)), int(round(bb * 255))
    return recolour


# ---- drawing --------------------------------------------------------------------

class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.defs, self.body, self.n = w, h, [], [], 0

    def grad(self, stops, x2=0, y2=1, radial=False):
        self.n += 1
        gid = "g%d" % self.n
        st = "".join('<stop offset="%g" stop-color="%s"/>' % (o, hexc(c)) for o, c in stops)
        if radial:
            self.defs.append('<radialGradient id="%s" cx="0.4" cy="0.35" r="0.75">%s</radialGradient>' % (gid, st))
        else:
            self.defs.append('<linearGradient id="%s" x1="0" y1="0" x2="%g" y2="%g">%s</linearGradient>' % (gid, x2, y2, st))
        return "url(#%s)" % gid

    def add(self, s):
        self.body.append(s)

    def rect(self, x, y, w, h, rx=0, fill="none", stroke=None, sw=1):
        self.add('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" ry="%g" fill="%s"%s/>' % (
            x, y, w, h, rx, rx, fill, (' stroke="%s" stroke-width="%g"' % (stroke, sw)) if stroke else ""))

    def text(self):
        return ('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg id="bitmap:%d-32" width="%d" height="%d" version="1.1" viewBox="0 0 %d %d" '
                'xmlns="http://www.w3.org/2000/svg">\n<defs>%s</defs>\n%s\n</svg>\n' % (
                    self.w, self.w, self.h, self.w, self.h, "".join(self.defs), "\n".join(self.body)))


def svg_size(text):
    m = re.search(r'<svg[^>]*\bwidth="(\d+)"[^>]*\bheight="(\d+)"', text, re.S)
    return int(m.group(1)), int(m.group(2))


# push buttons: normal, hot, pressed, disabled, defaulted, defaulted (animating)
def gen_button(era, w, h, count):
    s = Svg(w, h)
    sh = h // count
    for i in range(count):
        y = i * sh
        if era == "horizon":
            edge = "#c9c7ba" if i == 3 else "#003c74"
            if i == 2:
                fill = s.grad([(0, (226, 223, 214)), (0.8, (234, 232, 224)), (1, (242, 241, 236))])
            elif i == 3:
                fill = hexc((245, 244, 234))
            else:
                fill = s.grad([(0, (255, 255, 255)), (0.55, (246, 245, 240)), (0.85, (235, 232, 222)), (1, (214, 208, 197))])
            s.rect(0.5, y + 0.5, w - 1, sh - 1, 3, fill, edge)
            if i == 1:      # hot: an orange glow inside the edge
                s.rect(1.75, y + 1.75, w - 3.5, sh - 3.5, 2, "none", s.grad([(0, (255, 240, 207)), (0.5, (252, 210, 121)), (1, (229, 151, 0))]), 1.6)
            elif i in (4, 5):   # defaulted: a blue one
                s.rect(1.75, y + 1.75, w - 3.5, sh - 3.5, 2, "none", s.grad([(0, (206, 231, 255)), (0.5, (152, 184, 239)), (1, (105, 130, 238))]), 1.6)
        else:
            edge = {0: "#707070", 1: "#3c7fb1", 2: "#2c628b", 3: "#adb2b5", 4: "#3399ff", 5: "#3399ff"}[i]
            if i == 1:
                stops = [(0, (234, 246, 253)), (0.5, (217, 240, 252)), (0.5, (190, 230, 253)), (1, (167, 217, 245))]
            elif i == 2:
                stops = [(0, (229, 244, 252)), (0.5, (196, 229, 246)), (0.5, (152, 209, 239)), (1, (104, 179, 219))]
            elif i == 3:
                stops = [(0, (244, 244, 244)), (1, (244, 244, 244))]
            else:
                stops = [(0, (242, 242, 242)), (0.5, (235, 235, 235)), (0.5, (221, 221, 221)), (1, (207, 207, 207))]
            s.rect(0.5, y + 0.5, w - 1, sh - 1, 3, s.grad(stops), edge)
            if i != 3:     # the gloss: a light line inside the edge
                s.rect(1.5, y + 1.5, w - 3, sh - 3, 2, "none", "#ffffff" if i != 2 else "#9ed3ef", 1)
                s.add('<rect x="1.5" y="%g" width="%g" height="%g" rx="2" fill="none" stroke="#ffffff" stroke-opacity="0.35"/>' % (y + 1.5, w - 3, sh - 3))
            if i in (4, 5):
                s.rect(1.5, y + 1.5, w - 3, sh - 3, 2, "none", "#9fd4f3", 1)
    return s.text()


def _check_mark(s, x, y, size, colour, width_scale=1.0):
    k = size / 13.0
    pts = [(3.0, 6.5), (5.4, 9.0), (10.0, 3.8)]
    d = "M" + " L".join("%g,%g" % (x + px * k, y + py * k) for px, py in pts)
    s.add('<path d="%s" fill="none" stroke="%s" stroke-width="%g" stroke-linecap="round" stroke-linejoin="round"/>' % (
        d, colour, max(1.6, 2.1 * k * width_scale)))


# check boxes: unchecked, checked, mixed, implicit, excluded -- each normal, hot, pressed, disabled
def gen_checkbox(era, w, h, count):
    size = w
    s = Svg(w, h)
    for i in range(count):
        kind, state = divmod(i, 4)
        y = i * size
        if era == "horizon":
            edge = "#cac8bb" if state == 3 else "#1c5180"
            if state == 3:
                fill = hexc((255, 255, 255))
            elif state == 2:
                fill = s.grad([(0, (176, 176, 167)), (1, (227, 225, 214))], 1, 1)
            else:
                fill = s.grad([(0, (220, 220, 215)), (1, (255, 255, 255))], 1, 1)
            s.rect(0.5, y + 0.5, size - 1, size - 1, 0, fill, edge)
            if state == 1:
                s.rect(1.75, y + 1.75, size - 3.5, size - 3.5, 0, "none", s.grad([(0, (255, 240, 207)), (1, (248, 179, 48))], 1, 1), 1.5)
            mark = "#cac8bb" if state == 3 else "#21a121"
        else:
            edge = {0: "#8e8f8f", 1: "#3c7fb1", 2: "#2c628b", 3: "#bcbcbc"}[state]
            inner = {0: ((203, 207, 213), (246, 246, 246)), 1: ((177, 223, 253), (233, 247, 254)),
                     2: ((126, 196, 234), (209, 236, 251)), 3: ((244, 244, 244), (244, 244, 244))}[state]
            s.rect(0.5, y + 0.5, size - 1, size - 1, 0, "#f6f6f6" if state != 3 else "#f4f4f4", edge)
            pad = max(2, round(size * 0.18))
            s.rect(pad, y + pad, size - 2 * pad, size - 2 * pad, 0, s.grad([(0, inner[0]), (1, inner[1])], 1, 1), "#ffffff" if state != 3 else None, 0.5)
            mark = "#a0a0a0" if state == 3 else "#25349a" if state else "#1f2c5e"
        k = size / 13.0
        if kind in (1, 3):
            _check_mark(s, 0, y, size, mark if kind == 1 else hexc(shade(parse(mark), 0.45)))   # implicit: paler
        elif kind == 2:
            m = round(3 * k)
            s.rect(m, y + m, size - 2 * m, size - 2 * m, 0, mark)
        elif kind == 4:
            c = size / 2.0
            r = 3.0 * k
            s.add('<path d="M%g,%g L%g,%g M%g,%g L%g,%g" stroke="%s" stroke-width="%g" stroke-linecap="round"/>' % (
                c - r, y + c - r, c + r, y + c + r, c + r, y + c - r, c - r, y + c + r, mark, max(1.5, 1.8 * k)))
    return s.text()


# radio buttons: unchecked, checked -- each normal, hot, pressed, disabled
def gen_radio(era, w, h, count):
    size = w
    s = Svg(w, h)
    for i in range(count):
        kind, state = divmod(i, 4)
        y = i * size
        c = size / 2.0
        r = size / 2.0 - 0.5
        if era == "horizon":
            edge = "#cac8bb" if state == 3 else "#1c5180"
            fill = "#ffffff" if state == 3 else s.grad([(0, (255, 255, 255) if state != 2 else (210, 210, 200)), (1, (214, 214, 206) if state != 2 else (236, 235, 228))], 0, 1)
            s.add('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="%s"/>' % (c, y + c, r, fill, edge))
            if state == 1:
                s.add('<circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="1.5"/>' % (
                    c, y + c, r - 1.3, s.grad([(0, (255, 240, 207)), (1, (248, 179, 48))])))
            dot = s.grad([(0, (98, 222, 98)), (1, (22, 150, 22))], radial=True) if state != 3 else "#cac8bb"
        else:
            edge = {0: "#8e8f8f", 1: "#3c7fb1", 2: "#2c628b", 3: "#bcbcbc"}[state]
            inner = {0: ((203, 207, 213), (246, 246, 246)), 1: ((177, 223, 253), (233, 247, 254)),
                     2: ((126, 196, 234), (209, 236, 251)), 3: ((244, 244, 244), (244, 244, 244))}[state]
            s.add('<circle cx="%g" cy="%g" r="%g" fill="#f6f6f6" stroke="%s"/>' % (c, y + c, r, edge))
            s.add('<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (c, y + c, r - max(1.5, size * 0.13), s.grad([(0, inner[0]), (1, inner[1])], 1, 1)))
            dot = s.grad([(0, (114, 196, 248)), (0.6, (36, 120, 206)), (1, (14, 80, 160))], radial=True) if state != 3 else "#a8a8a8"
        if kind == 1:
            s.add('<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (c, y + c, max(2.0, size * 0.22), dot))
    return s.text()


# scroll bar arrow buttons: up, down, left, right (normal, hot, pressed,
# disabled each), then the four "hover" states -- the bar hot, not the button
def gen_arrows(era, w, h, count):
    s = Svg(w, h)
    sh = h // count
    for i in range(count):
        state = 4 if i >= 16 else i % 4
        y = i * sh
        if era == "horizon":
            if state == 3:
                s.rect(0.5, y + 0.5, w - 1, sh - 1, 2, "#f4f3ee", "#e8e6dc")
                continue
            top, bottom = {0: ((232, 239, 253), (190, 207, 250)), 1: ((253, 254, 255), (208, 223, 253)),
                           2: ((170, 191, 246), (204, 219, 252)), 4: ((240, 245, 254), (200, 215, 251))}[state]
            s.rect(0.5, y + 0.5, w - 1, sh - 1, 2, s.grad([(0, top), (1, bottom)], 1, 1), "#ffffff")
            s.rect(0.5, y + 0.5, w - 1, sh - 1, 2, "none", "#b4c8f6" if state != 2 else "#8ea9ea", 0.6)
        else:
            if state in (0, 3):     # at rest the track shows through, as on that desktop
                s.rect(0, y, w, sh, 0, s.grad([(0, (233, 233, 236)), (1, (243, 243, 245))], 1, 0))
                continue
            edge, stops = {1: ("#3c7fb1", [(0, (227, 244, 252)), (0.45, (214, 238, 251)), (0.5, (169, 219, 246)), (1, (164, 210, 238))]),
                           2: ("#2c628b", [(0, (205, 234, 249)), (0.45, (163, 214, 241)), (0.5, (110, 187, 227)), (1, (93, 170, 214))]),
                           4: ("#9a9a9a", [(0, (243, 243, 243)), (0.45, (232, 232, 232)), (0.5, (214, 214, 214)), (1, (222, 222, 222))])}[state]
            s.rect(0.5, y + 0.5, w - 1, sh - 1, 2, s.grad(stops, 1, 0), edge)
    return s.text()


def gen_thumb(era, w, h, count, vertical):
    s = Svg(w, h)
    sh = h // count
    for i in range(count):
        y = i * sh
        state = i      # normal, hot, pressed, disabled, hover
        if vertical:
            x0, y0, ww, hh, gx, gy = 1, y, w - 2, sh, 1, 0
        else:
            x0, y0, ww, hh, gx, gy = 0, y + 1, w, sh - 2, 0, 1
        if era == "horizon":
            if state == 3:
                s.rect(x0 + 0.5, y0 + 0.5, ww - 1, hh - 1, 2, "#f4f3ee", "#e8e6dc")
                continue
            a, b = {0: ((201, 216, 252), (182, 201, 249)), 1: ((218, 230, 254), (197, 213, 252)),
                    2: ((166, 188, 245), (186, 204, 248)), 4: ((210, 223, 253), (188, 206, 250))}[state]
            s.rect(x0 + 0.5, y0 + 0.5, ww - 1, hh - 1, 2, s.grad([(0, a), (1, b)], gx, gy), "#ffffff")
            s.rect(x0 + 0.5, y0 + 0.5, ww - 1, hh - 1, 2, "none", "#9cb4f2", 0.6)
        else:
            if state == 3:
                s.rect(x0 + 0.5, y0 + 0.5, ww - 1, hh - 1, 2, "#f4f4f4", "#d6d6d6")
                continue
            edge, stops = {0: ("#9a9a9a", [(0, (243, 243, 243)), (0.45, (232, 232, 232)), (0.5, (214, 214, 214)), (1, (222, 222, 222))]),
                           4: ("#8a8a8a", [(0, (246, 246, 246)), (0.45, (236, 236, 236)), (0.5, (220, 220, 220)), (1, (228, 228, 228))]),
                           1: ("#3c7fb1", [(0, (227, 244, 252)), (0.45, (214, 238, 251)), (0.5, (169, 219, 246)), (1, (164, 210, 238))]),
                           2: ("#2c628b", [(0, (205, 234, 249)), (0.45, (163, 214, 241)), (0.5, (110, 187, 227)), (1, (93, 170, 214))])}[state]
            s.rect(x0 + 0.5, y0 + 0.5, ww - 1, hh - 1, 2, s.grad(stops, gx, gy), edge)
    return s.text()


def gen_track(era, w, h, count):
    s = Svg(w, h)
    sh = h // count
    colours = {"horizon": [(254, 254, 251), (244, 243, 236), (226, 224, 214), (254, 254, 251), (250, 249, 244)],
               "glass": [(236, 236, 238), (226, 226, 230), (206, 206, 210), (240, 240, 240), (232, 232, 235)]}[era]
    for i in range(count):
        s.rect(0, i * sh, w, sh, 0, hexc(colours[i % len(colours)]))
    return s.text()


def gen_groupbox(era, w, h, count):
    s = Svg(w, h)
    e = ERAS[era]
    s.rect(0.5, 0.5, w - 1, h - 1, 3, "none", hexc(e["group_edge"]))
    if era == "glass":
        s.rect(1.5, 1.5, w - 3, h - 3, 2, "none", "#ffffff")
    return s.text()


GENERATORS = [
    (re.compile(r"^button$"), gen_button),
    (re.compile(r"^checkbox_\d+px$"), gen_checkbox),
    (re.compile(r"^radiobutton_\d+px$"), gen_radio),
    (re.compile(r"^scrollbar_arrows$"), gen_arrows),
    (re.compile(r"^scrollbar_thumb_vertical$"), lambda e, w, h, c: gen_thumb(e, w, h, c, True)),
    (re.compile(r"^scrollbar_thumb_horizontal$"), lambda e, w, h, c: gen_thumb(e, w, h, c, False)),
    (re.compile(r"^scrollbar_(lower|upper)_track_(vertical|horizontal)$"), gen_track),
    (re.compile(r"^groupbox$"), gen_groupbox),
]


def image_counts(lines):
    """ImageCount of each image file BLUE_INI names (1 if none is given)."""
    counts, section, count, files = {}, None, 1, []
    for line in lines + ['"[End]\\r\\n"']:
        if re.match(r'^"(?:\\r\\n)*\[', line):
            for f in files:
                counts[f] = count
            count, files = 1, []
        m = re.search(r'ImageCount\s*=\s*(\d+)', line)
        if m:
            count = int(m.group(1))
        for f in dark.IMAGE_REF.findall(line):
            files.append(f)
    return counts


# ---- the INI -------------------------------------------------------------------------

def set_props(lines, section, props):
    """Give SECTION these "Name = value" properties: replace the ones it has,
    add the others after its header; a section it lacks is added at the end."""
    out, i, found = [], 0, False
    want = {k.lower(): (k, v) for k, v in props}
    pat = re.compile(r'^"(?:\\r\\n)*\[%s\]\\r\\n"$' % re.escape(section), re.I)
    while i < len(lines):
        line = lines[i]
        out.append(line)
        if pat.match(line.strip()):
            found = True
            done = set()
            i += 1
            while i < len(lines) and not re.match(r'^"(?:\\r\\n)*\[', lines[i]) and lines[i].strip():
                m = re.match(r'^"\s*([A-Za-z0-9]+)\s*=', lines[i])
                if m and m.group(1).lower() in want:
                    k, v = want[m.group(1).lower()]
                    out.append('"%s = %s\\r\\n"' % (k, v))
                    done.add(m.group(1).lower())
                else:
                    out.append(lines[i])
                i += 1
            for key, (k, v) in want.items():
                if key not in done:
                    out.append('"%s = %s\\r\\n"' % (k, v))
            continue
        i += 1
    if not found:
        out += ["", '"[%s]\\r\\n"' % section] + ['"%s = %s\\r\\n"' % (k, v) for k, v in props]
    return out


def rgb(c):
    return "%d %d %d" % c


def era_ini(era, lines):
    e = ERAS[era]
    recolour = recolour_for(era)
    out, section = [], ""
    for line in lines:
        sec = re.match(r'^"(?:\\r\\n)*\[([^\]]*)\]', line)
        if sec:
            section = sec.group(1).lower()
        m = dark.RGB_LINE.match(line)
        if m:
            name = m.group(2)
            r, g, b = int(m.group(4)), int(m.group(5)), int(m.group(6))
            if section == "sysmetrics" and name.lower() == "background":
                pass    # the desktop's colour is the wallpaper's business
            elif section == "sysmetrics" and name in e["sys"]:
                r, g, b = e["sys"][name]
            else:
                r, g, b = recolour(r, g, b)
            line = "%s%s%s%d %d %d%s" % (m.group(1), name, m.group(3), r, g, b, m.group(7))
        out.append(dark.IMAGE_REF.sub(lambda mm: "%s_%s.bmp" % (era, mm.group(1)), line))
    p = e["progress"]
    edits = [
        ("Button.Pushbutton(Pressed)", [("TextColor", "0 0 0")]),
        ("Button.Groupbox", [("TextColor", rgb(e["group_text"]))]),
        ("Progress", [("ProgressChunkSize", str(p["chunk"])), ("ProgressSpaceSize", str(p["space"])),
                      ("BorderColor", rgb(p["bar_edge"])), ("FillColor", rgb(p["fill"][1]))]),
        ("Progress.Bar", [("FillColor", rgb(p["bar"]))]),
        ("Progress.BarVert", [("FillColor", rgb(p["bar"]))]),
    ]
    for part in ("Progress.Chunk", "Progress.ChunkVert", "Progress.Fill", "Progress.FillVert"):
        edits.append((part, [("BorderSize", "0"), ("FillType", "VertGradient" if not part.endswith("Vert") else "HorzGradient"),
                             ("GradientColor1", rgb(p["fill"][0])), ("GradientColor2", rgb(p["fill"][1])),
                             ("FillColor", rgb(p["fill"][1]))]))
    for cls in ("Edit", "ComboBox", "ListBox", "ListView", "TreeView", "Explorer::ListView", "Explorer::TreeView"):
        edits.append((cls, [("BorderColor", rgb(e["edge"]))]))
    edits.append(("Edit.EditText(Focused)", [("BorderColor", rgb(e["edge"]) if era == "horizon" else "61 123 173")]))
    for section_name, props in edits:
        out = set_props(out, section_name, props)
    return out


# ---- the images -------------------------------------------------------------------

def recolour_svg(text, recolour, glyph_map=None):
    def hexsub(m):
        h = m.group(0)
        if glyph_map:
            full = "#" + "".join(ch * 2 for ch in h[1:]) if len(h) == 4 else h
            if full.lower() in glyph_map:
                return glyph_map[full.lower()]
        return hexc(recolour(*parse(h)))
    return rounded.HEX.sub(hexsub, text)


def main(d):
    tree = os.path.normpath(os.path.join(d, "..", ".."))
    rc = open(os.path.join(d, "light.rc"), encoding="utf-8").read()
    blue = dark.ini_block(rc, "BLUE_INI")
    counts = image_counts(blue)
    images = sorted(f for f in os.listdir(d) if f.startswith("blue_") and f.endswith(".bmp"))
    res = ["/* Generated by wine-sg theme/eras.py from light.rc and the Light",
           " * images -- do not edit; change the script. */", ""]
    for era in ("horizon", "glass"):
        res += ["/* %s theme */" % era.capitalize(), "%s_INI TEXTFILE" % era.upper(), "{"] + era_ini(era, blue) + ["}", ""]
    for era in ("horizon", "glass"):
        recolour = recolour_for(era)
        for f in images:
            stem = f[5:-4]
            svg = os.path.join(d, "blue_%s.svg" % stem)
            out_svg = os.path.join(d, "%s_%s.svg" % (era, stem))
            out_bmp = os.path.join(d, "%s_%s.bmp" % (era, stem))
            if os.path.exists(svg):
                text = open(svg, encoding="utf-8").read()
                gen = next((g for pat, g in GENERATORS if pat.match(stem)), None)
                if gen:
                    w, h = svg_size(text)
                    text = gen(era, w, h, counts.get(stem, 1))
                elif stem.startswith("scrollbar_arrow_glyphs"):
                    text = recolour_svg(text, recolour, ERAS[era]["glyph"])
                else:
                    text = recolour_svg(text, recolour)
                with open(out_svg, "w", encoding="utf-8") as fh:
                    fh.write(text)
                rounded.render(tree, out_svg, out_bmp)
            else:
                saved = dark.remap
                dark.remap = recolour
                try:
                    dark.remap_bmp(os.path.join(d, f), out_bmp)
                finally:
                    dark.remap = saved
            name = "%s_%s.bmp" % (era, stem)
            res += ["/* @makedep: %s */" % name, '%s BITMAP "%s"' % (name.upper().replace(".", "_"), name), ""]
    with open(os.path.join(d, "eras.rc"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(res))
    print("eras.py: %d images per era" % len(images))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: eras.py DIR (dlls/light.msstyles)")
    main(sys.argv[1])
