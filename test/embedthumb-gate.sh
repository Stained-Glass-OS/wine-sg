#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's hover preview (wine-sg 7802; David 2026-11-15):
#   1. a framed Linux program's preview shows the program, not a black
#      client area: PrintWindow(PW_RENDERFULLCONTENT) of its frame takes the
#      client area from X, where the program draws, as for another
#      process's window (win32u dce.c; sg_embed.c marks the frame)
#   2. a right-click on its button puts the preview away, and the pointer
#      resting on the button while the menu is up does not bring it back
#      over the menu (programs/explorer/systray.c)
# A stand-in X program (embedconf-app.c: a green window) framed by the
# taskbar as linuxembed-gate does; the pointer rests on its button (the
# preview: green in it), then a right-click (the menu, no preview for 3 s).
#   WINE=/opt/wine-sg/bin/wine test/embedthumb-gate.sh
# Mutants: SG_MUTANT_PRINT_EMBED_BLACK (dlls/win32u/dce.c) fails 1;
# SG_MUTANT_THUMB_OVER_MENU (programs/explorer/systray.c) fails 2.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xwininfo import convert cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedthumb.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/app" "$HERE/embedconf-app.c" -lX11 || { echo "FAIL  embedconf-app did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/embedthumb-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${EMBEDTHUMB_DISPLAY:-}" ]; then
    Xvfb ":$EMBEDTHUMB_DISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$EMBEDTHUMB_DISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
L="$T/app.log"; : > "$L"
"$T/app" "$L" & AP=$!
i=0; while ! grep -q '^main ' "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(awk '$1 == "main" {print $2; exit}' "$L")
printf '%d shown - DockTest\tDock Test\nEND\n' "$XID" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
cd "$WINEPREFIX/drive_c"
root_id=$(xwininfo -root | awk '/Window id:/ {print $4; exit}')
k=0; while [ $k -lt 80 ]; do
    p=$(xwininfo -id "$XID" -tree 2>/dev/null | awk '/Parent window id:/ {print $4; exit}')
    [ -n "$p" ] && [ "$p" != "$root_id" ] && break
    sleep 0.25; k=$((k + 1))
done
[ $k -lt 80 ] || { fail "the window was not framed"; exit 1; }
sleep 2
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
B=$(probe button SgLinuxWindow "Dock Test")
case "$B" in none|"") fail "no taskbar button for the framed window"; exit 1 ;; esac
set -- $B; BX=$((($1 + $3) / 2)); BY=$((($2 + $4) / 2))
green() {   # pixels of the program's colour (0x40a040) in the rectangle L T R B
    import -window root -crop "$(($3 - $1))x$(($4 - $2))+$1+$2" "$T/crop.png" 2>/dev/null
    convert "$T/crop.png" -fuzz 12% -fill white -opaque '#40A040' -fill black +opaque white -format '%[fx:int(mean*w*h)]' info: 2>/dev/null
}

# 1. the pointer rests on its button: its preview shows it
xdotool mousemove 500 300; sleep 0.5
xdotool mousemove "$BX" "$BY"; sleep 0.3; xdotool mousemove $((BX + 2)) "$BY"; sleep 2.5
th=$(probe thumb)
case "$th" in
visible=1*)
    set -- $th; n=$(green "$2" "$3" "$4" "$5")
    [ "${n:-0}" -gt 500 ] && pass "the hover preview of a framed Linux program shows it ($n pixels of its colour)" \
        || fail "the hover preview shows no program ($n pixels of its colour; black client area)" ;;
*) fail "no hover preview over its button ($th)" ;;
esac

# 2. a right-click: the preview goes, and does not come back over the menu
# (the pointer moved a little on the button first, as a hand does: that asks
# for the next hover)
xdotool mousemove $((BX + 4)) "$BY"; sleep 0.1; xdotool click 3; sleep 0.4
m=$(probe menu); t1=$(probe thumb)
sleep 3
t2=$(probe thumb); m2=$(probe menu)
[ "$m" = menu=1 ] && [ "$m2" = menu=1 ] && [ "$t1" = visible=0 ] && [ "$t2" = visible=0 ] \
    && pass "a right-click on its button: the menu, and no preview over it (also with the pointer resting there)" \
    || fail "right-click: $m then $m2, preview at once $t1, 3 s later $t2 (want menu=1, visible=0)"
xdotool key Escape; sleep 0.5
[ $RC = 0 ] && echo "embedthumb: all passed"
exit $RC
