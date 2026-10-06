#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A thread idle for a while and just woken is not hung (1128). wineserver
# took a thread for hung when it had not read its messages for five seconds
# and was not waiting for them at that moment -- so one that had waited idle
# and was woken by something else (an X event, a handle) was "hung" while it
# did that work, and SendMessageTimeout(SMTO_ABORTIFHUNG) to it failed at once
# without the message: a new display scale's WM_SETTINGCHANGE never reached a
# notice idle on screen, which stayed at the old scale (hidpi-live-check).
#   1. a sent message to a thread that answers right away: delivered
#   2. the same thread idle 7 s, woken, at work 1.5 s: delivered when it is
#      back (it was dropped: ok=0)
#
#   WINE=/opt/wine-sg/bin/wine test/hungwake-gate.sh
#   Mutant: SG_MUTANT_HUNG_AFTER_WAIT (server/queue.c): 2 fails.
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
T=$(mktemp -d /var/tmp/sg-hungwake.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/hungwake-probe.c" || { fail "the probe did not build"; exit 1; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run"
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" "$T/probe.exe" idle >/dev/null 2>&1 &
sleep 3
r=$("$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
[ "$r" = "ok=1 r=77" ] && pass "a thread that answers gets the message ($r)" || fail "a thread that answers: '$r' (want ok=1 r=77)"
sleep 7
r=$("$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
[ "$r" = "ok=1 r=77" ] && pass "idle 7 s, woken and at work 1.5 s: not taken for hung, the message delivered ($r)" \
    || fail "idle 7 s, woken: '$r' (want ok=1 r=77; ok=0: taken for hung, the message dropped)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
