#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A DirectComposition swap chain with alpha shows its alpha (patches/sg/0605).
# Chrome draws its menus (the three-dot menu) as a composition swap chain with
# premultiplied alpha on a popup, with a soft shadow in a transparent margin;
# drawn into an ordinary window, that margin was a big black frame (David).
# Committing such a swap chain makes the window a sheet of glass (0435): a
# 32-bit ARGB X window whose transparent pixels stay transparent. Under Xvfb,
# with Wine's own d3d11 (wined3d over the server's GL).
#
#   WINE=/opt/wine-sg/bin/wine test/dcompalpha-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
CXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
DPY="${DPY:-$((700 + $$ % 200))}"
RC=0 XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo xwininfo xwd python3 "$CXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dcompalpha.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$CXX" -O2 -o "$T/dcompalpha-probe.exe" "$HERE/dcompalpha-probe.cpp" -ld3d11 -ldxgi -ldcomp -luser32 -luuid \
    -static-libgcc -static-libstdc++ || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

winpath=$(printf '%s' "$T" | sed 's|/|\\|g')
timeout -s KILL 180 "$WINE" "$T/dcompalpha-probe.exe" "Z:$winpath" >"$T/out" 2>/dev/null </dev/null & PP=$!
i=0; while ! grep -q '^GLASS\|^NO' "$T/out" && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
tr -d '\r' <"$T/out" | sed 's/^/      /'
grep -q '^NO' "$T/out" && { echo "SKIP: no d3d11 composition here ($(tr -d '\r' <"$T/out"))"; exit 77; }
set -- $(tr -d '\r' <"$T/out" | grep '^GLASS')
[ "${2:-0}" = 1 ] && pass "a swap chain with alpha makes its window a sheet of glass" || fail "glass prop: ${2:-?}"
if [ -n "${3:-}" ] && [ "${3:-0}" != 0 ]; then
    got=$(xwininfo -id "0x$3" | awk '/Depth:/{printf "depth %s ", $2}')
    px=$(xwd -silent -id "0x$3" | python3 -c '
import struct, sys
d = sys.stdin.buffer.read()
h = struct.unpack(">25I", d[:100])
off, bpl = h[0] + h[19] * 12, h[12]
p = d[off + 50 * bpl + 100 * 4: off + 50 * bpl + 100 * 4 + 4]
print((p[::-1] if h[7] == 0 else p).hex())')
    echo "      $got pixel $px"
    case "$got" in *"depth 32 "*) pass "its X window has a 32-bit ARGB visual";; *) fail "visual: $got";; esac
    [ "$px" = 00000000 ] && pass "a transparent pixel stays transparent (not black)" || fail "pixel $px (ff000000 is the black frame)"
else fail "no X window: $*"; fi
: >"$T/done"; wait "$PP" 2>/dev/null
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
