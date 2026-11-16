#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Another process's child window keeps what it draws (wine-sg 7801; DevToys 2,
# David's dev box 2026-11-15: the window's client area black). WPF hosts
# WebView2 in a WS_EX_TRANSPARENT child; Chromium's GPU process draws the page
# into a window of its own in it, with GDI (our DirectComposition), only when
# the page changes. Two things lost it:
#   - WPF's drawing over its whole client (transparent children are not
#     clipped) was flushed from its window surface over the page: a window of
#     another process is now left out of the surface, as a swap chain's is
#     (server window.c); the window's process tells the surface's process
#     when it moves or appears (win32u)
#   - WPF gives its window an X window with alpha for Mica after the page was
#     drawn; the GPU process's DC kept the old X window (a window of its own
#     is not "another process's", so its DC was not brought up to date): a
#     DC of a window under another process's top-level is updated each time
#     (win32u dce.c)
# The probe (xprocdc-probe.c): a host (white top-level, a transparent child),
# a render process (a red child in it, drawn once, again when asked). Checked
# where the red child is: red at first; red after the host fills its client
# blue through its DC; the host made glass (32-bit X window), the red drawn
# again shows, and stays after another blue fill.
#   WINE=/opt/wine-sg/bin/wine test/xprocdc-gate.sh
# Mutants: SG_MUTANT_FOREIGN_CHILD_IN_SURFACE (server/window.c) and
# SG_MUTANT_FOREIGN_CHILD_NO_NOTIFY (dlls/win32u/window.c) fail "after the
# host's fill"; SG_MUTANT_XPROC_TOP_STALE_DC (dlls/win32u/dce.c) fails "drawn
# again after glass".
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb xwininfo import "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xprocdc.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
"$MINGW" -O2 -o "$T/xprocdc.exe" "$HERE/xprocdc-probe.c" -ldwmapi -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${XPROCDC_DISPLAY:-}" ]; then
    Xvfb ":$XPROCDC_DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$XPROCDC_DISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/xprocdc.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2
"$WINE" xprocdc.exe host 'C:\slot.txt' >/dev/null 2>&1 &
i=0; while [ ! -s slot.txt ] && [ $i -lt 60 ]; do sleep 0.25; i=$((i + 1)); done
SLOT=$(tr -d '\r\n' < slot.txt)
[ -n "$SLOT" ] || { fail "the host did not start"; exit 1; }
"$WINE" xprocdc.exe render "$SLOT" 'C:\centre.txt' >/dev/null 2>&1 &
i=0; while [ ! -s centre.txt ] && [ $i -lt 60 ]; do sleep 0.25; i=$((i + 1)); done
read -r CX CY < centre.txt
[ -n "${CY:-}" ] || { fail "the render process did not start"; exit 1; }
sleep 1
px() { import -window root -crop "1x1+$CX+$CY" -depth 8 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-F]\{6\}'; }
sig() { "$WINE" xprocdc.exe signal "$1" >/dev/null 2>&1; sleep 1.5; }

p0=$(px)
[ "$p0" = "#FF0000" ] && pass "the other process's child shows what it drew ($p0)" || fail "at first: $p0 (want #FF0000)"
sig cover; p1=$(px)
[ "$p1" = "#FF0000" ] && pass "after the host's fill over its whole client, still the child's ($p1)" \
    || fail "after the host's fill: $p1 (want #FF0000; #0000FF: the host's surface flushed over it)"
sig glass
depth=$(xwininfo -root -tree | awk '/"XProc Host"/ {print $1; exit}' | xargs -r xwininfo -id 2>/dev/null | awk '/Depth:/ {print $2}')
sig redraw; p2=$(px)
[ "$depth" = 32 ] || echo "NOTE  the host's X window after glass has depth '$depth' (expected 32)"
[ "$p2" = "#FF0000" ] && pass "the host made glass (X window depth $depth), drawn again it shows ($p2)" \
    || fail "drawn again after glass: $p2 (want #FF0000: its DC kept the host's old X window)"
sig cover; p3=$(px)
[ "$p3" = "#FF0000" ] && pass "...and stays after another fill ($p3)" || fail "after glass and a fill: $p3 (want #FF0000)"
"$WINE" xprocdc.exe signal quit >/dev/null 2>&1
[ $RC = 0 ] && echo "xprocdc: all passed"
exit $RC
