#!/usr/bin/env python3
"""Checks a printer driver's output stream for the printer-driver corpus.

    check.py OUTPUT [REF.bmp]

Names the page description language of OUTPUT, checks that it parses (gs for
PostScript and PDF, gpcl6 for PCL 5 and PCL XL, our own readers for ZPL, ESC/P-R
and raster label printers), renders the first page at the reference's
resolution where it can and compares it with REF.bmp, the test page drawn by
sgprint ref.  Prints one line per finding and a last line:

    RESULT <language> valid=<yes|no|unknown> pages=<n> match=<0.00-1.00|->
"""
import os
import re
import subprocess
import sys
import tempfile

# GhostPCL: Debian does not package it; build it for testing only with
#   tar xzf ghostpdl-10.05.1.tar.gz && cd ghostpdl-10.05.1 &&
#   ./configure --without-x --disable-cups --disable-gtk && make -j4 gpcl6
GPCL6 = os.environ.get("GPCL6", "/var/tmp/pr-gpcl/ghostpdl-10.05.1/bin/gpcl6")
GS = os.environ.get("GS", "gs")


def detect(data):
    head = data[:4096]
    stripped = head
    # a PJL wrapper first: UEL, @PJL lines, then the language
    lang = None
    m = re.search(rb"@PJL\s+ENTER\s+LANGUAGE\s*=\s*([A-Z0-9]+)", data[:65536], re.I)
    if m:
        lang = m.group(1).upper().decode()
    if b"%!PS" in data[:65536] and lang in (None, "POSTSCRIPT", "PS"):
        return "PostScript"
    if b"%PDF-" in data[:4096]:
        return "PDF"
    if lang in ("PCLXL",) or b") HP-PCL XL" in data[:65536]:
        return "PCL XL"
    if lang in ("PCL",) or head.lstrip(b"\x00").startswith(b"\x1bE") or b"\x1b*r" in data[:65536]:
        return "PCL 5"
    if b"^XA" in data[:65536]:
        return "ZPL"
    if b"ESCPR" in data[:65536] or b"\x1b(R" in data[:65536]:
        return "ESC/P-R"
    if b"\x1bia" in data[:4096] or b"\x1biz" in data[:65536]:
        return "Brother raster"
    if head.startswith(b"\x1b@") or b"\x1b(G" in head:
        return "ESC/P"
    if lang:
        return "PJL+" + lang
    return "unknown"


def load_gray(path):
    from PIL import Image
    return Image.open(path).convert("L")


def compare(img, ref):
    """1.0 when the page's ink falls where the reference's does."""
    import numpy as np
    a = (np.asarray(img, dtype=np.float32) < 160).astype(np.float32)
    b = (np.asarray(ref, dtype=np.float32) < 160).astype(np.float32)
    h = min(a.shape[0], b.shape[0])
    w = min(a.shape[1], b.shape[1])
    if h < 10 or w < 10 or b.sum() == 0:
        return 0.0

    def blocks(x, n=4):
        x = x[: (x.shape[0] // n) * n, : (x.shape[1] // n) * n]
        return x.reshape(x.shape[0] // n, n, x.shape[1] // n, n).mean(axis=(1, 3))

    best = 0.0
    for dy in range(-24, 25, 4):
        for dx in range(-24, 25, 4):
            ya, yb = max(0, dy), max(0, -dy)
            xa, xb = max(0, dx), max(0, -dx)
            hh, ww = h - abs(dy), w - abs(dx)
            pa = blocks(a[ya:ya + hh, xa:xa + ww])
            pb = blocks(b[yb:yb + hh, xb:xb + ww])
            if pa.std() == 0 or pb.std() == 0:
                continue
            c = float(np.corrcoef(pa.ravel(), pb.ravel())[0, 1])
            best = max(best, c)
    return best


def render(cmd, outdir):
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        return None, "timeout"
    pages = sorted(f for f in os.listdir(outdir) if f.endswith(".png"))
    err = (r.stderr or b"").decode("latin-1")[-400:]
    if r.returncode != 0 and not pages:
        return None, "exit %d %s" % (r.returncode, err.strip().replace("\n", " | "))
    return [os.path.join(outdir, p) for p in pages], err


def _bits_image(raw, per_row, rows):
    from PIL import Image
    img = Image.new("L", (per_row * 8, rows), 255)
    px = img.load()
    for y in range(rows):
        for xb in range(per_row):
            i = y * per_row + xb
            if i >= len(raw):
                break
            v = raw[i]
            for bit in range(8):
                if v & (0x80 >> bit):
                    px[xb * 8 + bit, y] = 0
    return img


def zpl_pages(data):
    """the ^GF graphic fields of a ZPL stream, as images"""
    from PIL import Image
    text = data.decode("latin-1")
    imgs = []
    for m in re.finditer(r"\^GF([ABC]),(\d+),(\d+),(\d+),", text):
        kind, total, field, per_row = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
        if kind != "A" or per_row <= 0:
            continue
        rows = field // per_row
        body = text[m.end():]
        end = body.find("^")
        body = body[:end if end >= 0 else len(body)]
        if body.startswith((":Z64:", ":B64:")):
            import base64, zlib
            payload = body[5:].split(":")[0]
            raw = base64.b64decode(payload + "=" * (-len(payload) % 4))
            if body.startswith(":Z64:"):
                raw = zlib.decompress(raw)
            imgs.append(_bits_image(raw, per_row, rows))
            continue
        out = []
        prev = "0" * (per_row * 2)
        i = 0
        row = ""
        count = 0

        def flush_row(r):
            nonlocal prev
            r = r[: per_row * 2]
            out.append(r)
            prev = r

        while i < len(body) and len(out) < rows:
            c = body[i]
            if c in "\r\n ":
                i += 1
                continue
            if "G" <= c <= "Y" or "g" <= c <= "z":
                n = 0
                while i < len(body) and ("G" <= body[i] <= "Y" or "g" <= body[i] <= "z"):
                    ch = body[i]
                    n += (ord(ch) - ord("G") + 1) if ch <= "Y" else (ord(ch) - ord("g") + 1) * 20
                    i += 1
                if i < len(body):
                    row += body[i] * n
                    i += 1
            elif c == ",":
                flush_row(row + "0" * (per_row * 2 - len(row)))
                row = ""
                i += 1
            elif c == "!":
                flush_row(row + "F" * (per_row * 2 - len(row)))
                row = ""
                i += 1
            elif c == ":":
                flush_row(prev)
                row = ""
                i += 1
            elif c in "0123456789ABCDEFabcdef":
                row += c
                i += 1
            else:
                i += 1
            if len(row) >= per_row * 2:
                flush_row(row)
                row = ""
        width = per_row * 8
        img = Image.new("L", (width, len(out)), 255)
        px = img.load()
        for y, r in enumerate(out):
            r = (r + "0" * (per_row * 2))[: per_row * 2]
            for xb in range(per_row):
                v = int(r[xb * 2: xb * 2 + 2], 16)
                for bit in range(8):
                    if v & (0x80 >> bit):
                        px[xb * 8 + bit, y] = 0
        imgs.append(img)
    return imgs


def main():
    out = sys.argv[1]
    ref = sys.argv[2] if len(sys.argv) > 2 else None
    dpi = 100
    data = open(out, "rb").read() if os.path.exists(out) else b""
    if not data:
        print("RESULT none valid=no pages=0 match=-")
        return 1
    lang = detect(data)
    print("bytes %d language %s" % (len(data), lang))
    valid, pages, match = "unknown", 0, "-"
    refimg = load_gray(ref) if ref and os.path.exists(ref) else None
    with tempfile.TemporaryDirectory(dir="/var/tmp") as tmp:
        imgs = None
        if lang in ("PostScript", "PDF"):
            imgs, err = render([GS, "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER", "-sDEVICE=png16m", "-r%d" % dpi,
                                "-sOutputFile=%s/p%%03d.png" % tmp, out], tmp)
            if err and ("Error" in err or "error" in err):
                print("gs: " + err.strip().replace("\n", " | ")[:300])
                valid = "no"
        elif lang in ("PCL 5", "PCL XL") or lang.startswith("PJL+"):
            if os.path.exists(GPCL6):
                src = out
                # Kyocera's PRESCRIBE commands (!R! ... EXIT;) are for its printers, not
                # PCL; those outside the PCL XL streams go (inside, they are a comment's text)
                if b"!R!" in data:
                    inside = []
                    for m in re.finditer(rb"\) HP-PCL XL;", data):
                        end = data.find(b"\x1b%-12345X", m.start())
                        inside.append((m.start(), end if end >= 0 else len(data)))
                    out_parts, pos = [], 0
                    for m in re.finditer(rb"!R![^\x1b]*?EXIT;", data):
                        if any(a <= m.start() < b for a, b in inside):
                            continue
                        out_parts.append(data[pos:m.start()])
                        pos = m.end()
                    out_parts.append(data[pos:])
                    src = os.path.join(tmp, "noprescribe.prn")
                    with open(src, "wb") as f:
                        f.write(b"".join(out_parts))
                imgs, err = render([GPCL6, "-dNOPAUSE", "-dBATCH", "-sDEVICE=png16m", "-r%d" % dpi,
                                    "-sOutputFile=%s/p%%03d.png" % tmp, src], tmp)
                if err and ("rror" in err):
                    print("gpcl6: " + err.strip().replace("\n", " | ")[:300])
                    valid = "no"
        elif lang == "ZPL":
            z = zpl_pages(data)
            ok = data.count(b"^XA") == data.count(b"^XZ") and data.count(b"^XA") > 0
            print("zpl labels %d graphic fields %d" % (data.count(b"^XA"), len(z)))
            valid = "yes" if ok else "no"
            pages = data.count(b"^XA")
            if z:
                p = os.path.join(tmp, "z.png")
                z[0].save(p)
                imgs = [p]
        elif lang == "ESC/P-R":
            valid = "yes" if data.startswith(b"\x1b") else "unknown"
            pages = data.count(b"\x1bp")
        if imgs:
            pages = pages or len(imgs)
            if valid == "unknown":
                valid = "yes"
            if refimg is not None:
                from PIL import Image
                img = Image.open(imgs[0]).convert("L")
                if lang == "ZPL":
                    img = img.resize((max(1, img.width * dpi // 203), max(1, img.height * dpi // 203)))
                m = compare(img, refimg)
                match = "%.2f" % m
                try:
                    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
                    img.save(out + ".page1.png")
                except OSError:
                    pass
        elif imgs is not None and valid == "unknown":
            valid = "no"
    print("RESULT %s valid=%s pages=%s match=%s" % (lang, valid, pages, match))
    return 0


if __name__ == "__main__":
    sys.exit(main())
