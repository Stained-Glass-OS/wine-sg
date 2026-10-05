#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Direct2D DC render target leaves what it does not draw as the DC had it
# (patches/sg/0807): an opaque one (alpha mode IGNORE) starts from the DC's
# pixels -- through its own BeginDraw and through the ID2D1DeviceContext it
# gives out -- and a premultiplied one does not bring back an earlier
# draw's ink. Paint.NET 5's toolbar labels were black boxes.
#
#   WINE=/opt/wine-sg/bin/wine test/dcrt-opaque-gate.sh   (mutant SG_MUTANT_DCRT_NO_READBACK)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
CXX="${CXX:-x86_64-w64-mingw32-g++}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$CXX" >/dev/null || { echo "SKIP: $CXX missing"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing (Direct2D needs a display)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dcrt-opaque.XXXXXX)
n=180; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=":$n"
trap '"$WINESERVER" -k 2>/dev/null; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$CXX" -O2 -static -o "$T/probe.exe" "$HERE/dcrt-opaque-probe.cpp" -ld2d1 -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
line() { grep "^$1 " "$T/o" | head -1; }
for via in SELF CTX; do
    l=$(line "OPAQUE $via")
    case "$l" in *"keep=7030c0 ink=ff0000"*) pass "opaque target, $via: the DC's colour stays where nothing was drawn, the ink is there";;
                 *) fail "opaque $via: '$l'";; esac
    l=$(line "OPAQUE ${via}2")
    case "$l" in *"first=7030c0 keep=7030c0 ink=00ff00"*) pass "opaque target, $via, drawn again: the DC shows through, not the last draw";;
                 *) fail "opaque ${via}2: '$l'";; esac
    l=$(line "PREMUL ${via}2")
    case "$l" in *"first=7030c0 keep=7030c0 ink=00ff00"*) pass "premultiplied target, $via, drawn again: no ink from the first draw";;
                 *) fail "premultiplied ${via}2: '$l'";; esac
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; cat "$T/o"; }
exit "$RC"
