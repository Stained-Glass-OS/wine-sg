#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Task View's windows fly into place (patches/sg/0520): opening, each window
# moves from where it is on the screen to its place in the grid -- it changed
# in one frame (David 2026-09-29). The animation is stretched to 4 s
# (SG_TASKVIEW_ANIM_MS) so screenshots can see it: at 1.2 s the windows are on
# their way, at 6 s in place -- the two differ; drawn in one frame they do not.
#
#   WINE=/opt/wine-sg/bin/wine test/taskview-anim-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"   # absolute (the gate changes directory)
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb import compare "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tvanim.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
SG_TASKVIEW_ANIM_MS=4000 "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
cd "$WINEPREFIX/drive_c"
"$WINE" vdesk-probe.exe window Alpha 60 60 >/dev/null 2>&1 &
sleep 2
"$WINE" vdesk-probe.exe window Beta 520 200 >/dev/null 2>&1 &
sleep 3
"$WINE" vdesk-probe.exe hotkey taskview >/dev/null 2>&1 &
sleep 1.2; import -window root "$T/mid.png"
sleep 5;   import -window root "$T/end.png"
exists=$("$WINE" vdesk-probe.exe exists SgTaskView 2>/dev/null | tr -d '\r')
d=$(compare -metric AE "$T/mid.png" "$T/end.png" /dev/null 2>&1 | cut -d' ' -f1)
echo "      Task View: $exists; pixels differing between 1.2 s and 6 s: $d"
[ "$exists" = "exists=1" ] && pass "Win+Tab opens Task View" || fail "Task View did not open ($exists)"
[ "${d%.*}" -gt 2000 ] 2>/dev/null && pass "the windows are still on their way at 1.2 s (they fly into place)" \
    || fail "Task View is the same at 1.2 s and 6 s: no animation ($d px)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
