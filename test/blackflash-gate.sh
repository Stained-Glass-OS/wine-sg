#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows flash black when they open (patches/sg/0519). winex11 made every
# window with a black background, so the X server painted a window black the
# moment it was shown, before Wine drew it -- David: "sometimes windows flash
# black on open". With no background the server leaves what is under the window
# until the first paint, as Windows never shows a window it has not drawn.
#
# Deterministic: the program's Wine processes are stopped, so nothing Wine
# draws can hide what the X server itself shows. Shown again (open), a window
# must not be black; hidden (close), what it uncovers must not be black either
# (on Xvfb the desktop never painted black: that half is a guard).
#
#   WINE=/opt/wine-sg/bin/wine test/blackflash-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool xwininfo import python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-blackflash.XXXXXX); XP=; P=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { [ -n "$P" ] && kill -CONT $P 2>/dev/null; "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
"$WINE" notepad >/dev/null 2>&1 &
i=0; X=; while [ -z "$X" ] && [ $i -lt 60 ]; do sleep 0.5; X=$(xwininfo -root -tree 2>/dev/null | awk '/"Untitled - Notepad"/{print $1; exit}'); i=$((i + 1)); done
[ -n "$X" ] || { fail "Notepad did not open"; exit 1; }
sleep 2
# this prefix's Wine processes, and nothing else (not Xvfb, not a shell)
for p in $(pgrep -u "$(id -u)" .); do
    case "$(ps -o comm= -p "$p" 2>/dev/null)" in Xvfb|sh|bash|dash|timeout|xdotool|import|python3|xwininfo|sleep) continue ;; esac
    [ -r "/proc/$p/environ" ] && tr '\0' '\n' < "/proc/$p/environ" | grep -qx "WINEPREFIX=$WINEPREFIX" && P="$P $p"
done
black() {   # share of Notepad's area that is black in a screenshot
    import -window root "$T/s.png"
    python3 -c "
from PIL import Image; im=Image.open('$T/s.png').convert('RGB')
px=[im.getpixel((a,b)) for a in range(40,860,4) for b in range(60,600,4)]
print(int(100*sum(1 for p in px if max(p)<16)/len(px)))"
}
# open: hidden while Wine runs (the desktop repaints), then shown with Wine stopped
timeout 10 xdotool windowunmap "$X"; sleep 1
kill -STOP $P; sleep 0.3
timeout 10 xdotool windowmap "$X"; sleep 0.5
b=$(black); kill -CONT $P; sleep 2
echo "      open: ${b}% black"
[ "$b" -lt 5 ] && pass "a window being shown is not black before it draws (${b}%)" || fail "a window flashes black when it opens (${b}%)"
# close: hidden with Wine stopped, what it uncovers
kill -STOP $P; sleep 0.3
timeout 10 xdotool windowunmap "$X"; sleep 0.5
b=$(black); kill -CONT $P; P=
echo "      close: ${b}% black"
[ "$b" -lt 5 ] && pass "what a closing window uncovers is not black (${b}%)" || fail "a closing window leaves black (${b}%)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
