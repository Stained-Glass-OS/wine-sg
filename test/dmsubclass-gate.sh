#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct Manipulation and a window subclassed after it (patches/sg/9557):
# activated, subclassed by the program, deactivated, activated again -- a
# message must reach the program's procedures once each. Before, the second
# Activate subclassed the window over the program's subclass, which still
# called Direct Manipulation's: the first message went round until the stack
# ran out. Steam's web helper (Chromium's touchpad support does this to its
# windows) died that way every few seconds at its sign-in window, and Steam
# never showed a window.
#
#   WINE=/opt/wine-sg/bin/wine test/dmsubclass-gate.sh
# Mutant (directmanipulation, -DSG_MUTANT_DM_SUBCLASS_FORGOTTEN).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dmsubclass.XXXXXX)
n=180; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DISPLAY=":$n"
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; kill $XP 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/dmsubclass-probe.exe" "$HERE/dmsubclass-probe.c" -lole32 -luuid -luser32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
timeout 60 "$WINE" "$T/dmsubclass-probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
grep -q '^NOMANAGER' "$T/o" && { fail "no Direct Manipulation manager: $(cat "$T/o")"; exit 1; }
grep -q '^activate2 0$' "$T/o" && pass "activated, subclassed by the program, deactivated, activated again" \
    || fail "the activations: $(tr '\n' ' ' < "$T/o")"
l=$(grep '^RESULT ' "$T/o")
[ "$l" = "RESULT ret 7 class 1 sub 1" ] && pass "a message reaches the program's subclass and its window once each" \
    || fail "the message: '${l:-it never came back (the stack ran out)}' (want ret 7 class 1 sub 1)"
l=$(grep '^RESULT2 ' "$T/o")
[ "$l" = "RESULT2 ret 7 class 2 sub 2" ] && pass "and after the last Deactivate too" || fail "after deactivating: '$l'"
grep -q '^DONE' "$T/o" && pass "the window goes away cleanly" || fail "the probe did not finish"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
