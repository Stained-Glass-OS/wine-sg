#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Desktop wallpaper (patches/sg/0069): any picture format, Windows' styles.
#
# A PNG and a JPEG made here, red/green/blue/yellow quadrants, 2:1, on an
# 800x600 desktop: Fill (10) covers the desktop, cropping the sides; Fit (6)
# shows all of it, letterboxed; Stretch (2) fills it distorted; Center (0)
# and Tile (0 + TileWallpaper) as before. Checked on the X server's pixels.
# Setting a wallpaper with a window open must not paint over the window.
#
#   WINE=/opt/wine-sg/bin/wine test/wallpaper-gate.sh     (ARTIFACTS=DIR keeps screenshots)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wallpaper.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/wallpaper-probe.exe" "$HERE/wallpaper-probe.c" &&
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probes did not build"; exit 1; }
python3 - "$T" <<'EOF'
import sys
from PIL import Image
im = Image.new('RGB', (400, 200))
px = im.load()
for x in range(400):
    for y in range(200):
        px[x, y] = (255, 0, 0) if x < 200 and y < 100 else (0, 255, 0) if y < 100 else (0, 0, 255) if x < 200 else (255, 255, 0)
im.save(sys.argv[1] + '/quad.png')
im.save(sys.argv[1] + '/quad.jpg', quality=95)
EOF
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/*.exe "$T/quad.png" "$T/quad.jpg" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
WINEDEBUG=${EXPLORER_DEBUG:-err+all},trace+explorer "$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
# a window first (the desktop is drawn once there is something on it), below
# every point the checks look at
"$WINE" vdesk-probe.exe window Alpha 220 330 >/dev/null 2>&1 &
sleep 3
for st in "png 10 0" "jpg 10 0" "png 6 0" "png 2 0" "png 0 0" "png 0 1"; do
    set -- \$st
    "$WINE" wallpaper-probe.exe "C:\\\\quad.\$1" \$2 \$3 >> "$T/probe.out" 2>&1
    sleep 2
    import -window root "$T/\$1-\$2-\$3.png"
done
"$WINE" wallpaper-probe.exe "C:\\\\quad.jpg" 10 0 >> "$T/probe.out" 2>&1
sleep 2
import -window root "$T/over.png"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"

# colour name at a point: r g b y (the quadrants), k (background/other)
at() {
    convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)] %[fx:int(255*p{$2,$3}.g)] %[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null |
    awk '{ r=$1>200; g=$2>200; b=$3>200; n=$1<60&&$2<60&&$3<60
           if (r&&!g&&!b) print "r"; else if (g&&!r&&!b) print "g"; else if (b&&!r&&!g) print "b";
           else if (r&&g&&!b) print "y"; else print "k" }'
}
# the four corners, above the 40 px taskbar
look() { echo "$(at "$1" 5 5)$(at "$1" 794 5)$(at "$1" 5 550)$(at "$1" 794 550) ."; }

for f in png jpg; do
    v=$(look "$f-10-0")
    # Fill: the 2:1 picture is scaled to 1200x600 and cropped to 800 wide -- the corners are the quadrants
    [ "${v% *}" = "rgby" ] && pass "Fill ($f): the picture covers the desktop" || fail "Fill ($f): $v"
done
v=$(look png-6-0)
# Fit: 800x400, letterboxed -- corners are the background, the edges' middles too, but not the sides
[ "${v% *}" = "kkkk" ] && [ "$(at png-6-0 5 250)" = r ] && [ "$(at png-6-0 794 350)" = y ] \
    && pass "Fit: all of the picture, letterboxed top and bottom" || fail "Fit: $v, sides $(at png-6-0 5 250)$(at png-6-0 794 350)"
v=$(look png-2-0)
[ "${v% *}" = "rgby" ] && [ "$(at png-2-0 398 302)" = "b" ] && [ "$(at png-2-0 402 298)" = "g" ] \
    && pass "Stretch: the picture fills the desktop, split at its middle" || fail "Stretch: $v"
v=$(look png-0-0)
# Center: 400x200 in the middle -- (200,200) is red, the corners background
[ "${v% *}" = "kkkk" ] && [ "$(at png-0-0 205 205)" = r ] && [ "$(at png-0-0 595 395)" = y ] \
    && pass "Center: the picture at its own size, centred" || fail "Center: $v, inner $(at png-0-0 205 205)$(at png-0-0 595 395)"
v=$(look png-0-1)
[ "$(at png-0-1 5 5)" = r ] && [ "$(at png-0-1 405 5)" = r ] && [ "$(at png-0-1 5 205)" = r ] \
    && pass "Tile: repeated from the top left" || fail "Tile: $v"
w=$(convert "$T/over.png" -format '%[fx:int(255*p{300,500}.r)],%[fx:int(255*p{300,500}.g)],%[fx:int(255*p{300,500}.b)]' info:)
bar=$(convert "$T/over.png" -format '%[fx:int(255*p{300,590}.r)],%[fx:int(255*p{300,590}.g)],%[fx:int(255*p{300,590}.b)]' info:)
[ "$w" = "255,255,255" ] && pass "a window open while the wallpaper changes is not painted over" || fail "the window was painted over: $w"
[ "$bar" = "31,31,31" ] && pass "nor is the taskbar" || fail "the taskbar was painted over: $bar"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
