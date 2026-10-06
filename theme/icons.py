#!/usr/bin/env python3
"""icons.py TREE -- Stained Glass OS's own file and folder icons, as SVG
sources in the Wine tree, rendered to .ico by build.sh (theme/images.list).

David: the folder and file icons "look old", a text file shows Wine's
notepad -- a wine glass with a pencil. These are our own flat drawings in
the Windows 10 manner: a manila folder, an open one, a page with a folded
corner, a text page with lines, Notepad (a page under our accent band),
This PC (a monitor), Desktop, Documents, a fixed drive, and the file
dialogs' toolbar strip.

Each icon is one drawing on a 100-unit square, repeated at every size the
.ico carries (tools/buildimage cuts the rectangles named icon:SIZE-32). Line
widths are set per size in pixels, so the small sizes stay crisp and the
large ones do not look heavy. A new .ico gets an empty placeholder here, as
build.sh re-renders only files that exist.

Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later
"""
import os
import sys

SIZES = [256, 128, 96, 64, 48, 40, 32, 24, 20, 16]
if os.environ.get("SG_MUTANT_ICON_FRAMES"):   # test/iconframes-gate.sh's mutant: four sizes
    SIZES = [256, 48, 32, 16]
GAP = 8

FOLDER_BACK = "#E3A92E"
FOLDER_FRONT = "#F7C948"
FOLDER_EDGE = "#C98D16"
FOLDER_LIGHT = "#FBDD85"
PAGE = "#FFFFFF"
PAGE_EDGE = "#8C8C8C"
PAGE_FOLD = "#E4E4E4"
LINE = "#A0A0A0"
ACCENT = "#7B3FD0"


def px(size, pixels):
    """a width of so many pixels at this size, in the 100-unit drawing"""
    return pixels * 100.0 / size


def stroke_px(size):
    return 1.0 if size <= 48 else size / 48.0


def folder(size, open_=False):
    sw = px(size, stroke_px(size))
    back = (f'<path d="M6 20 H38 L46 28 H94 V86 H6 Z" fill="{FOLDER_BACK}" '
            f'stroke="{FOLDER_EDGE}" stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    if open_:
        front = (f'<path d="M18 40 H98 L86 86 H6 Z" fill="{FOLDER_FRONT}" '
                 f'stroke="{FOLDER_EDGE}" stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
        paper = f'<path d="M14 32 H86 V40 H14 Z" fill="{PAGE}"/>' if size >= 24 else ""
        return back + paper + front
    front = (f'<path d="M6 36 H94 V86 H6 Z" fill="{FOLDER_FRONT}" '
             f'stroke="{FOLDER_EDGE}" stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    light = ""
    if size >= 32:
        light = f'<path d="M{6 + sw:.2f} {36 + sw:.2f} H{94 - sw:.2f}" stroke="{FOLDER_LIGHT}" stroke-width="{sw:.2f}"/>'
    return back + front + light


def page(size, fold=True):
    sw = px(size, stroke_px(size))
    body = (f'<path d="M18 4 H62 L84 26 V96 H18 Z" fill="{PAGE}" stroke="{PAGE_EDGE}" '
            f'stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    corner = (f'<path d="M62 4 V26 H84 Z" fill="{PAGE_FOLD}" stroke="{PAGE_EDGE}" '
              f'stroke-width="{sw:.2f}" stroke-linejoin="round"/>') if fold else ""
    return body + corner


def text_lines(size, top, bottom, left=28, right=74, first_short=True):
    """lines of text, one or two pixels thick, snapped to pixel rows"""
    lw = px(size, 1.0 if size <= 32 else round(size / 32.0))
    step = max(px(size, 2 * (1.0 if size <= 32 else round(size / 32.0)) + (1 if size <= 24 else 2)), 8.0)
    out, y, i = [], top, 0
    while y <= bottom:
        r = right - 14 if (first_short and i % 3 == 2) else right
        out.append(f'<path d="M{left} {y:.2f} H{r}" stroke="{LINE}" stroke-width="{lw:.2f}"/>')
        y += step
        i += 1
    return "".join(out)


def text_document(size):
    return page(size) + text_lines(size, 38, 86)


def document(size):
    return page(size)


def notepad(size):
    sw = px(size, stroke_px(size))
    body = (f'<path d="M16 8 H84 V94 H16 Z" fill="{PAGE}" stroke="{PAGE_EDGE}" '
            f'stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    band = f'<path d="M16 8 H84 V26 H16 Z" fill="{ACCENT}"/>'
    rings = ""
    if size >= 32:
        for x in (30, 50, 70):
            rings += f'<circle cx="{x}" cy="17" r="{px(size, max(1.5, size / 20.0)):.2f}" fill="{PAGE}"/>'
    return body + band + rings + text_lines(size, 40, 86, left=26, right=74)


SCREEN = "#2F86D8"
BEZEL = "#2B2B2B"


def monitor(size):
    sw = px(size, stroke_px(size))
    return (f'<rect x="8" y="12" width="84" height="58" rx="4" fill="{BEZEL}"/>'
            f'<rect x="{13}" y="{17}" width="74" height="48" fill="{SCREEN}"/>'
            f'<path d="M13 65 L60 17 H87 V65 Z" fill="#4A9BE3"/>'
            f'<rect x="44" y="70" width="12" height="10" fill="#6E6E6E"/>'
            f'<rect x="28" y="80" width="44" height="7" rx="2" fill="#8A8A8A" stroke="#6E6E6E" stroke-width="{sw:.2f}"/>')


def desktop_folder(size):
    sw = px(size, stroke_px(size))
    art = folder(size)
    if size:
        art += (f'<rect x="30" y="46" width="40" height="28" rx="2" fill="{SCREEN}" stroke="{BEZEL}" '
                f'stroke-width="{sw:.2f}"/>')
    return art


def documents_folder(size):
    sw = px(size, stroke_px(size))
    back = (f'<path d="M6 20 H38 L46 28 H94 V86 H6 Z" fill="{FOLDER_BACK}" '
            f'stroke="{FOLDER_EDGE}" stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    paper = (f'<path d="M22 14 H78 V70 H22 Z" fill="{PAGE}" stroke="{PAGE_EDGE}" stroke-width="{sw:.2f}"/>'
             + (text_lines(size, 24, 44, left=30, right=70) if size >= 24 else ""))
    front = (f'<path d="M6 48 H94 V86 H6 Z" fill="{FOLDER_FRONT}" '
             f'stroke="{FOLDER_EDGE}" stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    return back + paper + front


def fixed_drive(size):
    sw = px(size, stroke_px(size))
    return (f'<rect x="6" y="34" width="88" height="40" rx="5" fill="#E2E2E2" stroke="#8C8C8C" stroke-width="{sw:.2f}"/>'
            f'<rect x="6" y="58" width="88" height="16" rx="3" fill="#C9C9C9"/>'
            f'<rect x="6" y="34" width="88" height="40" rx="5" fill="none" stroke="#8C8C8C" stroke-width="{sw:.2f}"/>'
            f'<rect x="14" y="63" width="30" height="5" fill="{ACCENT}"/>'
            f'<circle cx="82" cy="65.5" r="4" fill="#3CB371"/>')



# ---- user32's own window icons -------------------------------------------
# IDI_WINLOGO: our mark, four panes of coloured glass round a point (0441's
# drawing, now at every size). IDI_APPLICATION (OIC_SAMPLE): the icon of a
# program that has none of its own -- Wine's was a wine glass, which showed
# in title bars and on the taskbar; ours is a plain program window, as
# Windows' generic one is (our own drawing).

MARK = (("50,2 72,24 50,46 28,24", "#7B3FD0"), ("76,28 98,50 76,72 54,50", "#E0409A"),
        ("50,54 72,76 50,98 28,76", "#F0A030"), ("24,28 46,50 24,72 2,50", "#1FB0A0"))


def winlogo(size):
    return "".join(f'<polygon points="{p}" fill="{c}"/>' for p, c in MARK)


def application(size):
    sw = px(size, stroke_px(size))
    top, band = 14, (30 if size >= 24 else 34)
    frame = (f'<rect x="6" y="{top}" width="88" height="74" rx="{px(size, 1.5):.2f}" fill="{PAGE}" '
             f'stroke="{PAGE_EDGE}" stroke-width="{sw:.2f}"/>')
    bar = f'<path d="M6 {top} H94 V{band} H6 Z" fill="{ACCENT}"/>'
    dots = ""
    if size >= 32:
        r = px(size, max(1.0, size / 32.0))
        for x in (72, 80, 88):
            dots += f'<circle cx="{x}" cy="{(top + band) / 2:.2f}" r="{r:.2f}" fill="{PAGE}"/>'
    return frame + bar + dots

ICONS = {
    "dlls/shell32/resources/folder.svg": lambda s: folder(s),
    "dlls/shell32/resources/folder_open.svg": lambda s: folder(s, True),
    "dlls/shell32/resources/document.svg": document,
    "dlls/shell32/resources/sg_text.svg": text_document,
    "programs/notepad/notepad.svg": notepad,
    "dlls/shell32/resources/mycomputer.svg": monitor,
    "dlls/shell32/resources/desktop.svg": desktop_folder,
    "dlls/shell32/resources/mydocs.svg": documents_folder,
    "dlls/shell32/resources/drive.svg": fixed_drive,
    "dlls/user32/resources/oic_winlogo.svg": winlogo,
    "dlls/user32/resources/oic_sample.svg": application,
}

# ---- the file dialogs' toolbar strip (comctl32 IDB_VIEW_SMALL/LARGE) ----
# Twelve cells: large icons, small icons, list, details, sort by name, size,
# date and type, up one level, map and disconnect a network drive, new
# folder. Flat line glyphs, as Windows 10's toolbars draw them.

INK = "#404040"


def view_cell(i, u):
    """cell i drawn in a 16-unit square; u = pixels per unit"""
    w = 1.0 if u < 1.4 else 1.5
    def L(*pts):
        d = "M" + " L".join(f"{x * u:.2f} {y * u:.2f}" for x, y in pts)
        return f'<path d="{d}" fill="none" stroke="{INK}" stroke-width="{w * u / (1.5 if u >= 1.4 else 1):.2f}" stroke-linecap="square"/>'
    def R(x, y, ww, hh, fill="none", stroke=INK):
        return (f'<rect x="{x * u:.2f}" y="{y * u:.2f}" width="{ww * u:.2f}" height="{hh * u:.2f}" '
                f'fill="{fill}" stroke="{stroke}" stroke-width="{w * u / (1.5 if u >= 1.4 else 1):.2f}"/>')
    def down_arrow():
        return L((12.5, 2.5), (12.5, 13.5)) + L((10, 11), (12.5, 13.5), (15, 11))
    if i == 0:
        return R(1.5, 1.5, 5, 5) + R(9.5, 1.5, 5, 5) + R(1.5, 9.5, 5, 5) + R(9.5, 9.5, 5, 5)
    if i == 1:
        return "".join(R(1.5, y, 3, 3) + L((7, y + 1.5), (14.5, y + 1.5)) for y in (1.5, 6.5, 11.5))
    if i == 2:
        return "".join(R(x, y, 2, 2) + L((x + 3.5, y + 1), (x + 6.5, y + 1)) for x in (0.5, 8.5) for y in (2.5, 7.5, 12.5))
    if i == 3:
        return "".join(R(1.5, y, 2, 2) + L((5.5, y + 1), (14.5, y + 1)) for y in (2.5, 7.5, 12.5))
    if i == 4:   # by name: lines of falling length
        return L((1.5, 3.5), (9.5, 3.5)) + L((1.5, 7.5), (7.5, 7.5)) + L((1.5, 11.5), (5.5, 11.5)) + down_arrow()
    if i == 5:   # by size: squares growing
        return R(1.5, 10.5, 3, 3) + R(5.5, 7.5, 4, 6) + down_arrow()
    if i == 6:   # by date: a clock
        return (f'<circle cx="{5.5 * u:.2f}" cy="{8 * u:.2f}" r="{4.5 * u:.2f}" fill="none" stroke="{INK}" '
                f'stroke-width="{w * u / (1.5 if u >= 1.4 else 1):.2f}"/>' + L((5.5, 5.5), (5.5, 8), (7.5, 9.5)) + down_arrow())
    if i == 7:   # by type: a page
        return L((1.5, 2.5), (6.5, 2.5), (9.5, 5.5), (9.5, 13.5), (1.5, 13.5), (1.5, 2.5)) + down_arrow()
    if i == 8:   # up one level
        return L((8, 14.5), (8, 2)) + L((3, 7), (8, 2), (13, 7))
    if i in (9, 10):   # a network drive
        art = R(1.5, 2.5, 13, 6) + L((8, 8.5), (8, 12.5)) + L((2.5, 12.5), (13.5, 12.5)) + R(6.5, 11.5, 3, 2, fill=INK)
        if i == 10:
            art += (f'<path d="M{9.5 * u:.2f} {9.5 * u:.2f} L{15 * u:.2f} {15 * u:.2f} M{15 * u:.2f} {9.5 * u:.2f} '
                    f'L{9.5 * u:.2f} {15 * u:.2f}" stroke="#D13438" stroke-width="{1.6 * u:.2f}"/>')
        return art
    if i == 11:  # new folder: a manila folder, an accent plus
        return (f'<path d="M{1 * u:.2f} {4 * u:.2f} H{6 * u:.2f} L{7.5 * u:.2f} {5.5 * u:.2f} H{13 * u:.2f} V{14 * u:.2f} '
                f'H{1 * u:.2f} Z" fill="{FOLDER_FRONT}" stroke="{FOLDER_EDGE}" stroke-width="{u:.2f}"/>'
                f'<path d="M{12.5 * u:.2f} {0.5 * u:.2f} V{7.5 * u:.2f} M{9 * u:.2f} {4 * u:.2f} H{16 * u:.2f}" '
                f'stroke="{ACCENT}" stroke-width="{1.6 * u:.2f}"/>')
    return ""


def view_strip(cell):
    u = cell / 16.0
    width = cell * 12
    body = "".join(f'<g transform="translate({i * cell},0)">{view_cell(i, u)}</g>' for i in range(12))
    return ('<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n'
            '<!-- Stained Glass OS: generated by wine-sg theme/icons.py; our own drawing.\n'
            '     Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later -->\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" id="bitmap:{width}-32" width="{width}" height="{cell}" version="1.1">\n'
            + body + "\n</svg>\n")


STRIPS = {
    "dlls/comctl32/idb_view_small.svg": 16,
    "dlls/comctl32/idb_view_large.svg": 24,
}


def svg(draw):
    width = sum(SIZES) + GAP * (len(SIZES) - 1)
    height = max(SIZES)
    groups, rects, x = [], [], 0
    for s in SIZES:
        groups.append(f'<g transform="translate({x},0) scale({s / 100.0:.4f})">{draw(s)}</g>')
        rects.append(f'<rect id="icon:{s}-32" x="{x}" y="0" width="{s}" height="{s}" fill="none"/>')
        x += s + GAP
    return ('<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n'
            '<!-- Stained Glass OS: generated by wine-sg theme/icons.py; our own drawing.\n'
            '     Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later -->\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" version="1.1">\n'
            + "\n".join(groups) + "\n" + "\n".join(rects) + "\n</svg>\n")


def main(tree):
    for rel, draw in ICONS.items():
        path = os.path.join(tree, rel)
        with open(path, "w") as f:
            f.write(svg(draw))
        ico = path[:-4] + ".ico"
        if not os.path.exists(ico):
            open(ico, "wb").close()
        print(rel)
    for rel, cell in STRIPS.items():
        with open(os.path.join(tree, rel), "w") as f:
            f.write(view_strip(cell))
        print(rel)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
