#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DwmExtendFrameIntoClientArea (patches/sg/0435): negative margins make a
# window a "sheet of glass" -- what the program draws is shown with its own
# per-pixel alpha. Firefox draws its popups (permission doorhangers, menus,
# the notification prompt) this way, with a soft shadow in a transparent
# margin; without it that margin was an opaque black frame. Under Xvfb:
# the window gets a 32-bit ARGB visual and its pixels keep their alpha;
# zero margins put it back on the ordinary visual.
#
#   WINE=/opt/wine-sg/bin/wine test/glass-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${DPY:-$((700 + $$ % 200))}"
RC=0 XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo xwininfo xwd python3 "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-glass.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/glass-probe.exe" "$HERE/glass-probe.c" -ldwmapi -lgdi32 -luser32 \
    || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# depth and the alpha/colour of a transparent and an opaque pixel
look() {
    xwininfo -id "$1" | awk '/Depth:/{printf "depth %s ", $2}'
    xwd -silent -id "$1" | python3 -c '
import struct, sys
d = sys.stdin.buffer.read()
h = struct.unpack(">25I", d[:100])
off, bpl = h[0] + h[19] * 12, h[12]
px = lambda x, y: d[off + y * bpl + x * 4: off + y * bpl + x * 4 + 4]
print("left %s right %s" % (px(20, 50)[::-1 if h[7] == 0 else 1].hex(), px(180, 50)[::-1 if h[7] == 0 else 1].hex()))'
}
winpath=$(printf '%s' "$T" | sed 's|/|\\|g')
timeout -s KILL 180 "$WINE" "$T/glass-probe.exe" "Z:$winpath" >"$T/out" 2>/dev/null </dev/null & PP=$!
i=0; while ! grep -q '^GLASS' "$T/out" && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
tr -d '\r' <"$T/out" | sed 's/^/      /'
grep -q '^NULLMARGINS 80070057' "$T/out" && pass "no margins: E_INVALIDARG" || fail "no margins: $(grep NULLMARGINS "$T/out")"
set -- $(tr -d '\r' <"$T/out" | grep '^GLASS')
if [ "${2:-}" = 00000000 ] && [ -n "${3:-}" ]; then
    got=$(look "0x$3"); echo "      $got"
    case "$got" in *"depth 32 "*) pass "a sheet of glass gets a 32-bit ARGB visual";; *) fail "glass visual: $got";; esac
    case "$got" in *"left 00000000 right ff0000ff"*) pass "its pixels keep their alpha: transparent stays transparent, opaque stays opaque";;
                   *) fail "glass pixels: $got";; esac
else fail "glass: $*"; fi
: >"$T/next"
i=0; while ! grep -q '^OPAQUE' "$T/out" && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
set -- $(tr -d '\r' <"$T/out" | grep '^OPAQUE')
if [ "${2:-}" = 00000000 ] && [ -n "${3:-}" ]; then
    got=$(look "0x$3"); echo "      $got"
    case "$got" in *"depth 24 "*) pass "zero margins: back on the ordinary visual";; *) fail "opaque visual: $got";; esac
else fail "opaque: $*"; fi
: >"$T/done"
wait "$PP"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
