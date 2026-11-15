#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A message box without an icon (wine-sg 7784). Windows 10 draws the message on a white panel
# and the buttons on a grey strip beneath it (0454). Without an icon the panel was a few pixels
# high: the text ran onto the strip as a white band, and the empty icon control drew a white
# square on it (LibreHardwareMonitor's "PawnIO is not installed" box). Now the box has no icon
# control then, the panel is an icon's height at least, and the text ends above the strip, also
# when it has several lines. test/msgboxpanel-probe.c opens three boxes under Xvfb (no icon, one
# line; no icon, three lines; with an icon) and reads their layout and the strip's pixels.
#   WINE=/opt/wine-sg/bin/wine test/msgboxpanel-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for t in Xvfb x86_64-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msgboxpanel.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/msgboxpanel-probe.c" -lgdi32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
line() { printf '%s\n' "$out" | grep "^$1 " | head -1; }
check() {   # NAME ICON WHAT
    l=$(line "$1")
    echo "$l" | grep -q "icon=$2 " && pass "$3: the icon control is $( [ "$2" = 1 ] && echo shown || echo absent)" \
        || fail "$3: icon control: $l"
    echo "$l" | grep -q 'textbottom<=strip:1' && pass "$3: the text ends above the button strip" \
        || fail "$3: the text runs onto the strip: $l $(line "$1" | tail -1)"
    m=$(echo "$l" | sed -n 's/.*textmargin=\([0-9-]*\).*/\1/p')
    [ "${m:-0}" -ge 8 ] 2>/dev/null && pass "$3: the text has a margin above it ($m px)" || fail "$3: margin above the text '$m'"
    echo "$l" | grep -q 'strip=1' && pass "$3: the strip above the buttons is all face colour" \
        || fail "$3: the strip has other pixels: $(printf '%s\n' "$out" | grep "^$1 firstbad")"
}
check noicon1 0 "no icon, one line"
check noicon3 0 "no icon, three lines"
check icon 1 "an icon"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
