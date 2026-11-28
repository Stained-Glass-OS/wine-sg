#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A full-screen window of an emulated display mode (patches/sg/9556): the
# monitor keeps its own mode (1280x720 here), the program's 640x480 is shown
# scaled to the monitor keeping its shape -- 960x720 in the middle -- and black
# beside it, as Windows' display scaling shows a smaller mode. Before, the
# picture was drawn into the window's left corner and the rest of the monitor
# kept what was there before (the program's own green, a game's windowed frame,
# the taskbar).
#
# Runs test/emufs-probe.c on Xvfb with Wine's emulated mode changes
# (HKCU\Software\Wine\X11 Driver EmulateModeset=Y: the path the shell's desktop
# over the host's monitors takes, patches/sg/9555) and reads the screen.
#
#   WINE=/opt/wine-sg/bin/wine test/emufs-gate.sh
# Mutants (win32u/winex11, -DSG_MUTANT_x): EMULATED_SURFACE_CORNER (the picture
# in the corner), EMULATED_NO_BARS (nothing painted beside it).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xwd convert python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-emufs.XXXXXX)
n=180; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DISPLAY=":$n"
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winealsa.drv,winepulse.drv=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/emufs-probe.exe" "$HERE/emufs-probe.c" -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\X11 Driver' /v EmulateModeset /t REG_SZ /d Y /f >/dev/null 2>&1
"$WINESERVER" -w
timeout 60 "$WINE" "$T/emufs-probe.exe" 640 480 6000 > "$T/o" 2>/dev/null &
PP=$!
i=0; while [ $i -lt 300 ] && ! grep -q '^READY' "$T/o" 2>/dev/null; do python3 -c 'import time; time.sleep(0.1)'; i=$((i + 1)); done
grep -q '^READY' "$T/o" || { fail "the probe never showed its picture"; cat "$T/o"; exit 1; }
python3 -c 'import time; time.sleep(1.5)'
# pixels (x,y) on the 1280x720 screen: the bars are x < 160 and x >= 1120
xwd -root -silent | convert xwd:- -depth 8 rgb:- > "$T/screen.rgb"
px() { python3 - "$T/screen.rgb" "$1" "$2" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read(); x, y = int(sys.argv[2]), int(sys.argv[3]); i = (y * 1280 + x) * 3
r, g, b = d[i], d[i + 1], d[i + 2]
print('black' if r < 40 and g < 40 and b < 40 else 'red' if r > 200 and g < 60 and b < 60 else
      'green' if g > 200 and r < 60 and b < 60 else 'blue' if b > 200 and r < 60 and g < 60 else '%02x%02x%02x' % (r, g, b))
PY
}
tr -d '\r' < "$T/o" | grep -q 'CDS 640x480 0 screen 640x480' && pass "the mode change is taken: the screen is 640x480 to the program" \
    || fail "the mode change: $(tr -d '\r' < "$T/o" | head -1)"
want() { got=$(px "$1" "$2"); [ "$got" = "$3" ] && pass "$4 ($1,$2: $got)" || fail "$4 ($1,$2: $got, want $3)"; }
want 170 360 red   "the picture starts at the left edge of the mode's shape, 160 pixels in"
want 1110 360 red  "and ends at its right edge, 1120"
want 300 360 red   "its red frame is where the shape puts it"
want 860 360 blue  "its blue middle is in the middle of the monitor (400-880)"
want 80 360 black  "left of it is black, not the window's old picture"
want 1200 360 black "right of it is black"
want 80 20 black   "the corner beside it is black"
wait $PP
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
