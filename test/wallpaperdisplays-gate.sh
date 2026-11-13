#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The wallpaper on each display (patches/sg/7760), as Windows lays it out: over
# several displays (the shell's desktop over every monitor, 7749) "Fill" filled
# them all as one picture, a strip of it on each. Now each display shows all of
# it, filled; "Span" (WallpaperStyle 22) still goes across them.
#
# A headless sg-compositor with two outputs (1280x720 side by side) runs the
# desktop with a picture red on its left half and blue on its right; the
# desktop's X window is read back.
#
#   WINE=/opt/wine-sg/bin/wine test/wallpaperdisplays-gate.sh
#   SG_COMPOSITOR_BUILD=<dir with sg-compositor>, or SG_COMPOSITOR_SRC=<checkout> to build one
# Mutant: SG_MUTANT_WALLPAPER_ONE_PICTURE (user32/desktop.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in xwininfo import convert; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
SRC="${SG_COMPOSITOR_SRC:-$HERE/../../sg-compositor}"
[ -d "$SRC" ] || SRC="${SG_REAL_HOME:-$HOME}/Stained-Glass-OS/sg-compositor"

T=$(mktemp -d /var/tmp/sg-walldisp.XXXXXX)
stop() { [ -n "$CP" ] && { pkill -P "$CP" 2>/dev/null; kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; CP=""; }
cleanup() { "$WINESERVER" -k 2>/dev/null; stop; [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM

if [ -n "${SG_COMPOSITOR_BUILD:-}" ] && [ -x "$SG_COMPOSITOR_BUILD/sg-compositor" ]; then
    COMP=$SG_COMPOSITOR_BUILD/sg-compositor
elif [ -d "$SRC" ]; then
    meson setup "$T/comp" "$SRC" -Dman-pages=disabled --buildtype=release >"$T/meson.log" 2>&1 &&
        ninja -C "$T/comp" >"$T/ninja.log" 2>&1 || { echo "SKIP: cannot build the compositor"; exit 77; }
    COMP=$T/comp/sg-compositor
else
    echo "SKIP: no sg-compositor (SG_COMPOSITOR_BUILD or SG_COMPOSITOR_SRC)"; exit 77
fi

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
mkdir -p "$T/prefix" "$T/comp-xdg" "$T/wine-xdg"
chmod 700 "$T/comp-xdg" "$T/wine-xdg"
export XDG_RUNTIME_DIR="$T/wine-xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER

XDG_RUNTIME_DIR="$T/comp-xdg" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
    "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- \
    sh -c "echo \$DISPLAY > $T/disp; exec sleep 1200" >>"$T/comp.log" 2>&1 &
CP=$!
i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DISPLAY=$(cat "$T/disp" 2>/dev/null); export DISPLAY
[ -n "$DISPLAY" ] && [ "$DISPLAY" != ":0" ] || { echo "no display from the compositor"; exit 1; }
sleep 1

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
case "$(xwininfo -root 2>/dev/null | awk '/Width:/ {w=$2} /Height:/ {h=$2} END {print w "x" h}')" in
    2560x720) ;;
    *) echo "SKIP: the compositor did not make two 1280x720 outputs"; exit 77 ;;
esac
# a picture red on the left half, blue on the right, wider than the displays
convert -size 800x450 xc:'#FF0000' -size 800x450 xc:'#0000FF' +append "$WINEPREFIX/drive_c/wall.png" || { fail "no picture"; exit 1; }
D='HKCU\Control Panel\Desktop'
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v HostMonitors /d Y /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 2560x720 /f >/dev/null 2>&1
"$WINE" reg add "$D" /v Wallpaper /d 'C:\wall.png' /f >/dev/null 2>&1
"$WINE" reg add "$D" /v TileWallpaper /d 0 /f >/dev/null 2>&1
"$WINESERVER" -w

# shot STYLE NAME: the desktop with that wallpaper style, read back from its X window
shot() {
    "$WINE" reg add "$D" /v WallpaperStyle /d "$1" /f >/dev/null 2>&1; "$WINESERVER" -w
    timeout -s KILL 120 "$WINE" explorer /desktop=shell,2560x720 >/dev/null 2>&1 &
    i=0; while ! xwininfo -root -children 2>/dev/null | grep -q 'shell - Wine Desktop' && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
    sleep 6
    xwininfo -root -children > "$T/$2.tree" 2>&1
    id=$(awk '/Wine Desktop/ {print $1; exit}' "$T/$2.tree")
    # (an empty id makes import wait for a click)
    [ -n "$id" ] && import -window "$id" "$T/$2.png" 2>/dev/null
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
}
# a pixel's colour, red/blue/other, at y 300 (above the taskbar, clear of icons on the right)
px() {
    c=$(convert "$T/$1.png" -format "%[fx:int(255*p{$2,300}.r)],%[fx:int(255*p{$2,300}.b)]" info: 2>/dev/null)
    case "$c" in 2[0-9][0-9],[0-4][0-9]|2[0-9][0-9],[0-9]) echo red ;; [0-4][0-9],2[0-9][0-9]|[0-9],2[0-9][0-9]) echo blue ;; *) echo "other($c)" ;; esac
}

shot 10 fill
got="$(px fill 400) $(px fill 1100) $(px fill 1500) $(px fill 2400)"
[ "$got" = "red blue red blue" ] && pass "Fill: each display shows the whole picture, red then blue on each" ||
    fail "Fill: at 400/1100/1500/2400: $got"
shot 22 span
got="$(px span 400) $(px span 1100) $(px span 1500) $(px span 2400)"
[ "$got" = "red red blue blue" ] && pass "Span: one picture across both, red on the first, blue on the second" ||
    fail "Span: at 400/1100/1500/2400: $got"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
