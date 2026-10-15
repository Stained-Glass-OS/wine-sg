#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Sourced by the media smoke gates (patches/sg/4000-4027): builds
# test/media-smoke-probe.c for 64- and 32-bit, makes small media files (a
# WAV, an uncompressed AVI, a DVD-Video folder) and runs one probe section
# in a scratch prefix: media_smoke SECTION. Gates source scratch-home.sh
# first. WINE / WINESERVER as for the other gates.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"

media_smoke()
{
    section=$1
    for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
        command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
    done
    command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
    [ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
    T=$(mktemp -d /var/tmp/sg-media-smoke.XXXXXX)
    # a display of the gate's own (scratch-home.sh unset the person's)
    XVFB=""
    command -v xvfb-run >/dev/null && XVFB="xvfb-run -a -s -screen\ 0\ 1024x768x24"
    export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
    trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
    libs="-lole32 -loleaut32 -luuid -lwinmm -lmsacm32 -ldxva2 -lmfplat -lmf -lmfreadwrite -lmfuuid -lstrmiids -ldmoguids -luser32 -lavifil32 -lmsvfw32"
    TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/media-smoke-probe.c" $libs &&
        TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/media-smoke-probe.c" $libs ||
        { echo "FAIL  probe did not build"; exit 1; }
    python3 - "$T" <<'PY'
import sys, struct, os, math
d = sys.argv[1]
# 1 s of a 440 Hz tone, 8 kHz 16 bit mono
pcm = b''.join(struct.pack('<h', int(8000 * math.sin(2 * math.pi * 440 * i / 8000))) for i in range(8000))
with open(d + '/smoke.wav', 'wb') as f:
    fmt = struct.pack('<HHIIHH', 1, 1, 8000, 16000, 2, 16)
    f.write(b'RIFF' + struct.pack('<I', 4 + 8 + len(fmt) + 8 + len(pcm)) + b'WAVE')
    f.write(b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(pcm)) + pcm)
# an uncompressed AVI: 4 frames of 16x16 24 bit at 10 fps
w, h, n = 16, 16, 4
frame = bytes([0x40, 0x80, 0xc0]) * (w * h)
def chunk(tag, data): return tag + struct.pack('<I', len(data)) + data + (b'\0' if len(data) & 1 else b'')
def lst(tag, data): return b'LIST' + struct.pack('<I', len(data) + 4) + tag + data
avih = struct.pack('<14I', 100000, len(frame) * 10, 0, 0x10, n, 0, 1, len(frame), w, h, 0, 0, 0, 0)
strh = b'vids' + b'DIB ' + struct.pack('<IHHIIIIIIIIhhhh', 0, 0, 0, 0, 1, 10, 0, n, len(frame), 0xffffffff, 0, 0, 0, w, h)
strf = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(frame), 0, 0, 0, 0)
hdrl = lst(b'hdrl', chunk(b'avih', avih) + lst(b'strl', chunk(b'strh', strh) + chunk(b'strf', strf)))
movi = lst(b'movi', b''.join(chunk(b'00db', frame) for _ in range(n)))
idx = b''.join(struct.pack('<4sIII', b'00db', 0x10, 4 + i * (len(frame) + 8), len(frame)) for i in range(n))
body = b'AVI ' + hdrl + movi + chunk(b'idx1', idx)
with open(d + '/smoke.avi', 'wb') as f:
    f.write(b'RIFF' + struct.pack('<I', len(body)) + body)
# a DVD-Video folder
os.makedirs(d + '/dvd/VIDEO_TS')
for name, data in (('VIDEO_TS.IFO', b'DVDVIDEO-VMG' + bytes(2036)), ('VTS_01_0.IFO', b'DVDVIDEO-VTS' + bytes(2036)),
                   ('VTS_01_1.VOB', bytes(4096))):
    with open(d + '/dvd/VIDEO_TS/' + name, 'wb') as f: f.write(data)
PY
    mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    RC=0
    for p in p64 p32; do
        echo "== $p $section"
        out=$(cd "$T" && eval timeout -s KILL 120 $XVFB '"$WINE"' '"$T/$p.exe"' '"$section"' 2>/dev/null </dev/null | tr -d '\r')
        printf '%s\n' "$out"
        printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    done
    [ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
    exit "$RC"
}
