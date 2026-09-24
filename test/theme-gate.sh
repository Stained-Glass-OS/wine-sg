#!/bin/sh
# Text rendering and the scroll bars (patches/sg/0064, 0065), by pixels.
#
#   0064  The Windows font smoothing setting decides how text is drawn, even
#         where the host's fontconfig says otherwise: this runs with a
#         fontconfig that asks for greyscale on every font, and requires
#         ClearType text to have colour fringes, standard smoothing to be grey
#         only, and smoothing off to be two-colour only.
#   0065  The themed scroll bar is Stained Glass's: a flat track, a flat thumb
#         that darkens when hot, and none of Light's bordered boxes.
#
#   WINE=/opt/wine-sg/bin/wine test/theme-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: xvfb-run not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
FONT=$(fc-match -f '%{family[0]}' 'DejaVu Sans')
[ "$FONT" = "DejaVu Sans" ] || { echo "SKIP: DejaVu Sans (fonts-dejavu-core) not installed"; exit 77; }

T=$(mktemp -d /var/tmp/sg-theme.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

# The adversarial host preference: greyscale, no subpixel order, everywhere.
cat > "$T/fonts.conf" <<'EOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
  <match target="font">
    <edit name="antialias" mode="assign"><bool>true</bool></edit>
    <edit name="rgba" mode="assign"><const>none</const></edit>
  </match>
</fontconfig>
EOF
export FONTCONFIG_FILE="$T/fonts.conf"

printf '1 24 "%s"\n' "$HERE/theme-gallery.manifest" > "$T/g.rc"
"$WINDRES" "$T/g.rc" -O coff -o "$T/g.o" &&
"$MINGW" -municode -mwindows -O2 -o "$T/theme-gallery.exe" "$HERE/theme-gallery.c" "$T/g.o" \
    -lcomctl32 -luxtheme -lgdi32 -luser32 || { fail "gallery did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/theme-gallery.exe" "$C/"

# The message font: DejaVu Sans 9 pt, default quality (follows the setting).
python3 - "$T/font.reg" <<'EOF'
import struct, sys
lf = struct.pack('<5l8B', -12, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 0, 0) + 'DejaVu Sans'.encode('utf-16-le').ljust(64, b'\0')
open(sys.argv[1], 'w').write('Windows Registry Editor Version 5.00\n\n'
    '[HKEY_CURRENT_USER\\Control Panel\\Desktop\\WindowMetrics]\n'
    '"MessageFont"=hex:%s\n' % ','.join('%02x' % c for c in lf))
EOF
regimport() { "$WINE" regedit /S "$("$WINE" winepath -w "$1" 2>/dev/null)" >/dev/null 2>&1; "$WINESERVER" -w; }
regimport "$T/font.reg"

smoothing() {   # FontSmoothing TYPE
    printf 'Windows Registry Editor Version 5.00\n\n[HKEY_CURRENT_USER\\Control Panel\\Desktop]\n"FontSmoothing"="%s"\n"FontSmoothingType"=dword:%08x\n"FontSmoothingOrientation"=dword:00000001\n' \
        "$1" "$2" > "$T/s.reg"
    regimport "$T/s.reg"
}
render() {  # OUT
    (cd "$C" && timeout -s KILL 120 xvfb-run -a "$WINE" theme-gallery.exe --render "$1" >/dev/null 2>&1)
    "$WINESERVER" -w
    [ -s "$C/$1" ]
}
# Colour-fringed, grey and other pixels in the text rows (4..40, x < 390).
text_pixels() {
    python3 - "$C/$1" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
off, = struct.unpack_from('<I', d, 10)
w, h = 480, 120
fringe = grey = ink = 0
for y in range(4, 40):
    for x in range(0, 390):
        b, g, r = d[off + (y * w + x) * 4: off + (y * w + x) * 4 + 3]
        if (r, g, b) == (255, 255, 255): continue
        ink += 1
        if max(r, g, b) - min(r, g, b) > 24: fringe += 1
        elif (r, g, b) != (0, 0, 0): grey += 1
print(ink, fringe, grey)
EOF
}

smoothing 2 2
if render cleartype.bmp; then
    set -- $(text_pixels cleartype.bmp)
    [ "${1:-0}" -gt 500 ] && [ "${2:-0}" -gt 200 ] \
        && pass "ClearType: text has colour fringes though fontconfig says greyscale (ink $1, fringed $2)" \
        || fail "ClearType: no subpixel text (ink ${1:-?}, fringed ${2:-?}, grey ${3:-?})"
else fail "ClearType: nothing rendered"; fi

smoothing 2 1
if render standard.bmp; then
    set -- $(text_pixels standard.bmp)
    [ "${1:-0}" -gt 500 ] && [ "${2:-1}" -eq 0 ] && [ "${3:-0}" -gt 200 ] \
        && pass "standard smoothing: greyscale only (ink $1, grey $3)" \
        || fail "standard smoothing: ink ${1:-?}, fringed ${2:-?}, grey ${3:-?}"
else fail "standard smoothing: nothing rendered"; fi

smoothing 0 1
if render off.bmp; then
    set -- $(text_pixels off.bmp)
    [ "${1:-0}" -gt 300 ] && [ "${2:-1}" -eq 0 ] && [ "${3:-1}" -eq 0 ] \
        && pass "smoothing off: black and white only (ink $1)" \
        || fail "smoothing off: ink ${1:-?}, fringed ${2:-?}, grey ${3:-?}"
else fail "smoothing off: nothing rendered"; fi

# The scroll bar parts, drawn from the installed Light theme.
if [ -s "$C/cleartype.bmp" ]; then
    out=$(python3 - "$C/cleartype.bmp" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read()
off, = struct.unpack_from('<I', d, 10)
w = 480
def px(x, y):
    b, g, r = d[off + (y * w + x) * 4: off + (y * w + x) * 4 + 3]
    return '%02x%02x%02x' % (r, g, b)
seen = {px(x, y) for x in range(400, 417) for y in range(0, 100)}
print('arrow', px(402, 2), 'glyph', min((px(x, y) for x in range(400, 417) for y in range(0, 17)), key=lambda c: int(c[:2], 16)),
      'track', px(408, 27), 'thumb', px(408, 52), 'hot', px(428, 52), 'edge', px(400, 52), 'lower', px(408, 90),
      'bordergrey', 'aeaeae' in seen)
EOF
)
    echo "      $out"
    set -- $out
    [ "$2" = f0f0f0 ] && pass "arrow button is flat, on the track colour" || fail "arrow button is $2, not f0f0f0"
    [ "$4" = 606060 ] && pass "arrow glyph is solid dark grey" || fail "arrow glyph darkest pixel is $4"
    [ "$6" = f0f0f0 ] && [ "${14}" = f0f0f0 ] && pass "tracks are flat f0f0f0" || fail "tracks are $6 / ${14}"
    [ "$8" = cdcdcd ] && pass "thumb is flat cdcdcd" || fail "thumb is $8"
    [ "${10}" = a6a6a6 ] && pass "hot thumb darkens to a6a6a6" || fail "hot thumb is ${10}"
    [ "${12}" = f0f0f0 ] && pass "thumb sits in the track (inset)" || fail "thumb edge is ${12}"
    [ "${16}" = False ] && pass "no bordered boxes (Light's aeaeae border absent)" || fail "Light's border grey is still drawn"
fi

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
