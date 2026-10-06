#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar started at a raised display scale draws its text and icons
# once scaled (0880). Explorer is aware of the DPI: Wine gives it the menu
# font and SM_CXSMICON at the scale it started with, and the bar scaled
# them again (SG_PX) -- signed in at 175% on a 2736x1824 screen (a Surface
# Pro 7, David 2026-10-05) the clock's text was 1.75 x 1.75 times its
# size and the date ran off the bar's end. Under Xvfb, the shell's clock:
#   1. at 1920x1080 and 100%: the height of its two lines of text (the
#      reference)
#   2. at 2736x1824, the shell started at 175% (LogPixels 168): the same
#      text 1.5 to 2 times as tall (175%), not more (scaled twice), and the
#      bar 70 px
#   3. Task View's "New desktop" tile: 200 px wide at 100%, 350 at 175%
#      (it, the Alt+Tab switcher and the desktop switch indicator were
#      100%'s at every scale)
#
#   WINE=/opt/wine-sg/bin/wine test/barhidpi-gate.sh
#   (mutants SG_MUTANT_BAR_TWICE in systray.c, SG_MUTANT_TV_UNSCALED in vdesktop.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb import python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3 PIL missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-barhidpi.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/barscale-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/tv.exe" "$HERE/barhidpi-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
X=
xserver() {   # WxH
    "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && { kill "$XP" 2>/dev/null; wait "$XP" 2>/dev/null; }
    rm -f "$T/display"
    Xvfb -displayfd 3 -screen 0 "${1}x24" -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
    export DISPLAY=":$(cat "$T/display")"; X=$1
}
xserver 1920x1080
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
# the clock's text: the height of the rows with text in its corner of the bar
clock() {   # BAR-HEIGHT -> rows from the first to the last with the clock's (light) text
    "$WINE" explorer "/desktop=shell,$X" >/dev/null 2>&1 &
    sleep 10
    import -window root "$T/shot.png" 2>/dev/null
    python3 - "$T/shot.png" "$1" <<'EOS'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); w, h = im.size; bar = int(sys.argv[2])
rows = [y for y in range(h - bar, h) if any(min(im.getpixel((x, y))) > 180 for x in range(w - bar * 2, w))]
print(rows[-1] - rows[0] + 1 if rows else 0)
EOS
}
bar() { "$WINE" "$T/probe.exe" bar 2>/dev/null | tr -d '\r' | sed -n 's/.*height=\([0-9]*\).*/\1/p'; }
# Task View's "New desktop" tile (#333333): its width along a row 40 px down
tile() {
    "$WINE" explorer "/desktop=shell,$X" >/dev/null 2>&1 &
    sleep 8
    "$WINE" "$T/tv.exe" >/dev/null 2>&1; sleep 3
    import -window root "$T/tv.png" 2>/dev/null
    "$WINESERVER" -k; sleep 1
    python3 - "$T/tv.png" <<'EOS'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); w, h = im.size
best = run = 0
for x in range(w):
    run = run + 1 if im.getpixel((x, 40)) == (0x33, 0x33, 0x33) else 0
    best = max(best, run)
print(best)
EOS
}
"$WINE" explorer "/desktop=shell,$X" >/dev/null 2>&1 & sleep 8
b1=$(bar); "$WINESERVER" -k; sleep 1
t1=$(clock "${b1:-40}")
[ "${b1:-0}" = 40 ] && [ "${t1:-0}" -gt 10 ] && pass "1920x1080 at 100%: a 40 px bar, the clock's text $t1 px tall" \
    || fail "1080p: bar ${b1:-none}, clock text ${t1:-none}"
v1=$(tile)
[ "${v1:-0}" = 200 ] && pass "Task View at 100%: the New desktop tile 200 px wide" || fail "Task View at 100%: tile ${v1:-none} px (want 200)"
xserver 2736x1824
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 168 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer "/desktop=shell,$X" >/dev/null 2>&1 & sleep 8
b2=$(bar); "$WINESERVER" -k; sleep 1
t2=$(clock "${b2:-70}")
awk -v a="${t1:-0}" -v b="${t2:-0}" 'BEGIN { exit !(a > 0 && b / a >= 1.5 && b / a <= 2.0) }' && [ "${b2:-0}" = 70 ] \
    && pass "2736x1824, the shell started at 175%: a 70 px bar, the clock's text $t2 px tall ($(awk -v a="$t1" -v b="$t2" 'BEGIN { printf "%.2f", b / a }') times)" \
    || fail "175%: bar ${b2:-none}, clock text ${t2:-none} px against ${t1:-none} (want 1.5 to 2 times; scaled twice it is over 2, cut off by the bar)"
v2=$(tile)
[ "${v2:-0}" = 350 ] && pass "Task View at 175%: the New desktop tile 350 px wide" || fail "Task View at 175%: tile ${v2:-none} px (want 350)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
