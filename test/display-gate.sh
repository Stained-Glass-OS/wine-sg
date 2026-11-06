#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0207-0208: two winex11 display bugs.
#
#   1. (0207) A program on another X display than the desktop's owner (a
#      test's Xvfb while the desktop's explorer still runs on the display it
#      started on) died on an X BadWindow at its first cursor clip or
#      release: it used the desktop's clip window, which is on the other
#      display. Here: Xvfb A holds the desktop (explorer and a window), Xvfb
#      B runs a program that starts a second one, which clips and releases
#      the cursor; both must live, the second must exit with its own code.
#   2. (0208) In a virtual desktop (the shell's), a flush of the desktop's
#      surface painted over every window -- a new wallpaper covered the
#      windows and the taskbar. Here: green windows (painted once, so a later
#      repaint cannot hide anything), then the desktop repaints itself, then a
#      new wallpaper; the windows must still be green on the X server.
#
#   WINE=/opt/wine-sg/bin/wine test/display-gate.sh   (ARTIFACTS=DIR keeps screenshots)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v Xvfb >/dev/null && command -v xvfb-run >/dev/null && command -v import >/dev/null \
    || { echo "SKIP: needs Xvfb, xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-display.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
XA= XB=
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XA" ] && kill "$XA" 2>/dev/null
    [ -n "$XB" ] && kill "$XB" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/display-probe.exe" "$HERE/display-probe.c" -lgdi32 || { fail "the probe did not build"; exit 1; }

xvfb() {   # xvfb VAR: start an Xvfb, its display number in VAR's file
    Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$T/$1.fd" 2>/dev/null &
    echo $! > "$T/$1.pid"
    i=0; while [ ! -s "$T/$1.fd" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
}
xvfb a; XA=$(cat "$T/a.pid"); DA=:$(cat "$T/a.fd")
xvfb b; XB=$(cat "$T/b.pid"); DB=:$(cat "$T/b.fd")
[ "$DA" != ":" ] && [ "$DB" != ":" ] || { echo "SKIP: Xvfb did not start"; exit 77; }

mkdir -p "$WINEPREFIX"
DISPLAY=$DA timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/display-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1

# ---- 1. the desktop on display A, programs on display B ------------------------
DISPLAY=$DA "$WINE" display-probe.exe hold 40 >/dev/null 2>&1 &
sleep 4
out=$(DISPLAY=$DB timeout -s KILL 90 "$WINE" display-probe.exe spawn 2>&1 | tr -d '\r')
printf '%s\n' "$out" > "$T/spawn.out"
printf '%s\n' "$out" | sed 's/^/  /'
if printf '%s\n' "$out" | grep -q BadWindow; then fail "an X BadWindow killed a program on the second display"
else pass "no X BadWindow on the second display"; fi
printf '%s\n' "$out" | grep -qx 'clip=1' && printf '%s\n' "$out" | grep -qx 'unclip=1' \
    && pass "the second program clips and releases the cursor" || fail "cursor clip: $out"
printf '%s\n' "$out" | grep -qx 'second=alive window=1' && pass "and lives on with its window" || fail "the second program died"
printf '%s\n' "$out" | grep -qx 'second exit=7' && pass "and exits with its own code" || fail "second's exit: $out"
"$WINESERVER" -k 2>/dev/null
"$WINESERVER" -w

# ---- 2. the shell's virtual desktop: the desktop repaints, a window stays -------
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w
python3 -c "
import sys, struct
w = h = 64
row = bytes([30, 30, 200]) * w + b'\0' * ((4 - (w * 3) % 4) % 4)
data = row * h
hdr = b'BM' + struct.pack('<IHHI', 54 + len(data), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
open(sys.argv[1], 'wb').write(hdr + data)" "$WINEPREFIX/drive_c/red.bmp"
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v TileWallpaper /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v WallpaperStyle /d 0 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" display-probe.exe green 300 200 200 150 > "$T/green.out" 2>&1 &
i=0; while ! grep -q 'desktop redrawn' "$T/green.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
import -window root "$T/redraw.png"
"$WINE" display-probe.exe green 50 380 200 150 'C:\\\\red.bmp' > "$T/wallpaper.out" 2>&1 &
i=0; while ! grep -q 'desktop redrawn' "$T/wallpaper.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
import -window root "$T/wallpaper.png"
EOF
chmod +x "$T/session.sh"
DISPLAY=$DB timeout -s KILL 200 "$T/session.sh"
at() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)] %[fx:int(255*p{$2,$3}.g)] %[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
grep -q 'desktop redrawn' "$T/green.out" 2>/dev/null && pass "the desktop repainted itself with a window open" \
    || fail "the probe did not get to the desktop's repaint: $(cat "$T/green.out" 2>/dev/null)"
px=$(at redraw 400 275)
[ "$px" = "0 255 0" ] && pass "the window is still green: the desktop did not paint over it" \
    || fail "the window's centre is '$px' after the desktop repainted, not green"
tr -d '\r' < "$T/wallpaper.out" | grep -qx 'wallpaper=1' 2>/dev/null && pass "a new wallpaper is set with windows open" \
    || fail "the wallpaper: $(cat "$T/wallpaper.out" 2>/dev/null)"
px=$(at wallpaper 150 455)
[ "$px" = "0 255 0" ] && pass "the window is still green: the new wallpaper did not paint over it" \
    || fail "the window's centre is '$px' after the new wallpaper, not green"
px=$(at wallpaper 400 275)
[ "$px" = "0 255 0" ] && pass "nor over the first window" || fail "the first window's centre is '$px' after the new wallpaper"
px=$(at wallpaper 700 100)
[ "$px" = "200 30 30" ] && pass "and the desktop shows the wallpaper ($px)" || fail "the desktop is '$px', not the wallpaper"

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
