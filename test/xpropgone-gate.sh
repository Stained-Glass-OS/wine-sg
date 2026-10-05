#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window's X window destroyed under it (another thread or program did) and
# a property then set on it: the process lives on (patches/sg/0808). Xlib's
# own handler ended it -- the shell, explorer, on David's X1 when touches came
# quickly (2026-10-04: BadWindow on X_ChangeProperty; sg-session restarted it).
#
#   WINE=/opt/wine-sg/bin/wine test/xpropgone-gate.sh   (mutant SG_MUTANT_X_PROP_FATAL)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in "$MINGW" cc Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xpropgone.XXXXXX)
n=190; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=":$n"
trap '"$WINESERVER" -k 2>/dev/null; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/xpropgone-probe.c" || { fail "probe did not build"; exit 1; }
cc -O2 -o "$T/destroy" "$HERE/xpropgone-destroy.c" -lX11 || { fail "helper did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
mkdir -p "$T/flags"
flags=$("$WINE" winepath -w "$T/flags" 2>/dev/null | tr -d '\r')
timeout 90 "$WINE" "$T/probe.exe" "$flags" > "$T/o" 2>"$T/e" &
P=$!
w=0; while ! grep -q "^XID" "$T/o" 2>/dev/null && [ $w -lt 300 ]; do sleep 0.1; w=$((w + 1)); done
xid=$(sed -n 's/^XID //p' "$T/o" | tr -d '\r')
if [ -n "$xid" ] && [ "$xid" != 0 ]; then
    "$T/destroy" "$xid" && pass "the probe's X window $xid is destroyed from outside" || fail "could not destroy $xid"
else
    fail "no X window id from the probe: $(cat "$T/o")"
fi
touch "$T/flags/gone"
wait $P
grep -q "^ALIVE" "$T/o" && pass "setting its title (a property of the gone window) does not end the process" \
    || fail "the process ended: $(grep -m2 "X Error\|BadWindow\|Major opcode" "$T/e" | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
