#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A windowed ddraw primary keeps reaching the screen after an exclusive-mode round trip
# (patches/sg/6549). wined3d binds the GL context again when its window's client size changed
# since the last bind: Mesa's software rasterizer (llvmpipe: no GPU driver, VMs, Xvfb) learns a
# new window size only then or at a swap, and ddraw's windowed primary never swaps -- after
# DOSBox went back from fullscreen (Alt+Enter twice) every frame landed off its shrunk window.
# test/glresize-probe.c: blit, exclusive fullscreen with a mode change, back to NORMAL while the
# window is still screen-sized, shrink the window, blit again; read the pixel back from the screen.
#   WINE=/opt/wine-sg/bin/wine test/glresize-gate.sh
# Mutants: SG_MUTANT_GL_NO_REBIND_ON_RESIZE (dlls/wined3d/context_gl.c); SG_MUTANT_DDRAW_MODE_AFTER_SWAPCHAIN
# (dlls/ddraw/ddraw.c, 6113: back to NORMAL with no RestoreDisplayMode, as DOSBox does -- the desktop mode
# comes back before the swapchain is made again, or the window front buffer is the 640x480 mode's size).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-glresize.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
# a virtual desktop, so the 640x480 mode change is emulated whatever the X server offers
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d Default /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v Default /d 1024x768 /f >/dev/null 2>&1
"$WINESERVER" -w
for arch in x86_64 i686; do
    CC="$arch-w64-mingw32-gcc"
    command -v "$CC" >/dev/null || { echo "SKIP $arch: $CC missing"; continue; }
    TMPDIR=/var/tmp "$CC" -O1 -o "$WINEPREFIX/drive_c/glresize-$arch.exe" "$HERE/glresize-probe.c" -lddraw -ldxguid -lgdi32 -luser32 ||
        { fail "probe did not build ($arch)"; continue; }
    for mode in "" norestore; do
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" "C:\\glresize-$arch.exe" $mode 2>/dev/null | tr -d '\r')
    L="$arch${mode:+ (no RestoreDisplayMode, as DOSBox)}"
    echo "== $L"
    printf '%s\n' "$out" | sed 's/^/      /'
    check() {
        if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$L: $2"
        else fail "$L: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check windowed_blit "a windowed primary blit reaches the screen"
    check fullscreen    "exclusive fullscreen with a 640x480 mode"
    check restored      "the window is back at 400x300"
    check shown         "a primary blit after the round trip reaches the shrunk window"
    check done          "the probe ran to the end"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
