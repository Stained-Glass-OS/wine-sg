#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Another process's swap chain keeps its window's area (6500), and PrintWindow shows swap chains'
# frames (6501).
# Chromium's GPU process presents into the browser's windows (patches/sg/0586): into a child window
# its frames are copied into the browser's X window, into a top-level its client window is shown in
# place (6046). The browser's own repaint (an expose, a lock, erasing its background) was flushed
# over them -- white until the next present, which a still page never makes. Such a window's area is
# now left out of its window surface, as a window with a pixel format of its own is.
# PrintWindow with PW_RENDERFULLCONTENT (the taskbar's hover thumbnails, Task View, Alt+Tab,
# DwmRegisterThumbnail) copied only the window surface: GPU-presented windows (Opera, Dropbox) were
# white. The parts the surface leaves to swap chains are now taken from X (from the desktop
# compositor's pixmap of the window when sg-deskcomp runs, covered or not; where visible otherwise).
# The probe's host makes a top-level (white) with two child windows; a render process (at low
# integrity, as Chromium's sandboxed GPU process; the prefix's first user's host is elevated) presents red
# with Vulkan (as DXVK does for Chromium's Direct3D 11) into the big child, then into the top-level;
# the host repaints; another process prints the top-level, as it is and covered; the host moves the
# child away.
#   WINE=/opt/wine-sg/bin/wine test/xproc-child-gate.sh
#   (SG_DESKCOMP=<sg-deskcomp>, default /usr/libexec/stained-glass/sg-deskcomp: the covered print)
# Mutants: SG_MUTANT_FOREIGN_SURFACE_PAINTED (win32u window.c), SG_MUTANT_PRINT_NO_FRAMES (dce.c),
# SG_MUTANT_UIPI_CLIENT_SURFACE (server queue.c: the low-integrity renderer's message to an elevated host).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
DESKCOMP="${SG_DESKCOMP:-/usr/libexec/stained-glass/sg-deskcomp}"
for t in Xvfb xwd convert "$MINGWXX"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/include/vulkan/vulkan.h ] || { echo "SKIP: no Vulkan headers (libvulkan-dev)"; exit 77; }
[ -f /usr/share/vulkan/icd.d/lvp_icd.json ] || { echo "SKIP: no lavapipe (mesa-vulkan-drivers)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xprocchild.XXXXXX); XP=; CP=
# lavapipe: a software Vulkan whose frames reach a virtual X server's screen
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && kill "$CP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/inc"; ln -s /usr/include/vulkan "$T/inc/vulkan"; [ -d /usr/include/vk_video ] && ln -s /usr/include/vk_video "$T/inc/vk_video"
"$MINGWXX" -O2 -static -mwindows -I"$T/inc" -o "$T/xproc-child-probe.exe" "$HERE/xproc-child-probe.cpp" -lgdi32 -ladvapi32 ||
    { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/xproc-child-probe.exe" "$WINEPREFIX/drive_c/"
C="$WINEPREFIX/drive_c"
wait_phase() { i=0; while [ "$(cat "$C/phase.txt" 2>/dev/null)" != "$1" ] && [ $i -lt 400 ]; do sleep 0.1; i=$((i + 1)); done; }
px() { xwd -root -silent | convert xwd:- -crop "1x1+$1+$2" txt:- 2>/dev/null | tail -1 | sed -n 's/.*#\([0-9A-Fa-f]\{6\}\).*/\1/p' | tr a-f A-F; }
fail=0
# run MODE DESKTOP [compositor]: the child's centre is at 250,225 on the screen, a spot of the
# top-level outside its children at 120,380
run() {
    rm -f "$C/phase.txt" "$C"/print*.txt
    (cd "$C" && timeout 120 "$WINE" explorer "/desktop=$2,1024x700" "C:\\xproc-child-probe.exe" host "$1" \
        >"$T/run-$1.log" 2>&1 &)
    if [ -n "${3:-}" ]; then
        sleep 4
        mkdir -p "$T/comphome"
        (HOME="$T/comphome" XDG_CONFIG_HOME="$T/comphome" exec "$DESKCOMP" -display "$DISPLAY" >"$T/comp.log" 2>&1) & CP=$!
    fi
    wait_phase presented; sleep 1
    before=$(px 250 225); sbefore=$(px 120 380)
    wait_phase repainted; sleep 1
    after=$(px 250 225); safter=$(px 120 380)
    wait_phase printed
    print0=$(tr -d '\r' < "$C/print0.txt" 2>/dev/null); print1=$(tr -d '\r' < "$C/print1.txt" 2>/dev/null)
    moved=
    if [ "$1" = child ]; then wait_phase moved; sleep 1; moved=$(px 250 225); fi
    "$WINESERVER" -k 2>/dev/null; sleep 1
    [ -n "$CP" ] && { kill "$CP" 2>/dev/null; CP=; }
    echo "      ($1${3:+ composited}: child before=$before after=$after moved=$moved, spot before=$sbefore after=$safter;" \
         "print: $print0 / covered: $print1)"
}

run child gate
if [ "$before" != FF0000 ]; then echo "SKIP: the render process's frame is not on this X server's screen ($before)"; exit 77; fi
echo "PASS  another process's frame is shown in the host's child window"
case "$(tr -d '\r' < "$C/low.txt" 2>/dev/null)" in
    low=1) echo "PASS  (the render process runs at low integrity, as Chromium's sandboxed GPU process)";;
    *) echo "FAIL  the render process could not be started at low integrity"; fail=1;;
esac
if [ "$after" = FF0000 ]; then echo "PASS  the frame stays when the host repaints"
else echo "FAIL  the host's repaint wiped the frame in its child window ($after)"; fail=1; fi
if [ "$moved" = FFFFFF ]; then echo "PASS  where the child was is the host's own white after it moved"
else echo "FAIL  the place the child left is not the host's ($moved)"; fail=1; fi
if [ "$sbefore$safter" = FFFFFFFFFFFF ]; then echo "PASS  the rest of the host is its own white"
else echo "FAIL  the host's own part is not white ($sbefore $safter)"; fail=1; fi
case "$print0" in
    "ok=1 child=ff0000 spot=ffffff") echo "PASS  PrintWindow from another process shows the child's frame";;
    *) echo "FAIL  PrintWindow does not show the child's frame ($print0)"; fail=1;;
esac

run top gate
if [ "$sbefore" = FF0000 ] && [ "$safter" = FF0000 ]; then echo "PASS  a frame in the top-level stays when the host repaints"
else echo "FAIL  the host's repaint wiped the frame in its top-level ($sbefore -> $safter)"; fail=1; fi
case "$print0" in
    "ok=1 child=ffffff spot=ff0000") echo "PASS  PrintWindow shows another process's frame in the top-level, the host's own child over it";;
    *) echo "FAIL  PrintWindow of the top-level ($print0)"; fail=1;;
esac

if [ -x "$DESKCOMP" ]; then
    run child shell composited
    case "$print1" in
        "ok=1 child=ff0000 spot=ffffff") echo "PASS  composited, PrintWindow shows the frame of a covered window";;
        *) echo "FAIL  composited, PrintWindow of a covered window ($print1)"; fail=1;;
    esac
    if [ "$after" = FF0000 ]; then echo "PASS  composited, the frame stays when the host repaints"
    else echo "FAIL  composited, the host's repaint wiped the frame ($after)"; fail=1; fi
else
    echo "SKIP  covered windows (no sg-deskcomp at $DESKCOMP)"
fi
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
