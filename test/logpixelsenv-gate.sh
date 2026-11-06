#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A process takes the display scale handed to it in SG_LOGPIXELS over its
# account's (0882): sg-session starts the login screen, Setup and the
# first-run setup at the screen's recommended scale that way, with nothing
# written to the registry and no Wine process run before them -- wine reg
# run as the greeter's account just before the greeter was one cause of a
# login screen never drawn (release s9, 2026-10-05). Under Xvfb:
#   1. without it: the account's 96 DPI
#   2. SG_LOGPIXELS=168: 168 DPI; the account's LogPixels left as it was
#   3. a value out of range (1000, junk): ignored, 96
#
#   WINE=/opt/wine-sg/bin/wine test/logpixelsenv-gate.sh   (mutant SG_MUTANT_NO_LOGPIXELS_ENV)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-logpixelsenv.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/logpixelsenv-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1920x1080x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
dpi() { env ${1:+SG_LOGPIXELS="$1"} "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' | sed -n 's/.*dpi=\([0-9]*\).*/\1/p'; }
lp() { "$WINE" reg query 'HKCU\Control Panel\Desktop' /v LogPixels 2>/dev/null | tr -d '\r' | awk '$1 == "LogPixels" { print $3 }'; }
before=$(lp)
d=$(dpi "")
[ "$d" = 96 ] && pass "without SG_LOGPIXELS: 96 DPI, the account's" || fail "without: ${d:-none}"
d=$(dpi 168)
[ "$d" = 168 ] && [ "$(lp)" = "$before" ] && pass "SG_LOGPIXELS=168: 168 DPI, the account's LogPixels untouched ('$before')" \
    || fail "SG_LOGPIXELS=168: ${d:-none} DPI, LogPixels '$(lp)' (was '$before')"
d1=$(dpi 1000); d2=$(dpi junk)
[ "$d1" = 96 ] && [ "$d2" = 96 ] && pass "out of range or not a number: ignored" || fail "SG_LOGPIXELS=1000: $d1, junk: $d2"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
