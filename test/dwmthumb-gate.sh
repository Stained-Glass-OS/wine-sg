#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DWM thumbnails (patches/sg/5500): DwmRegisterThumbnail / UpdateThumbnailProperties /
# UnregisterThumbnail / QueryThumbnailSourceSize really draw the source window
# into the destination, read back from the destination's pixels (64- and 32-bit
# probe, test/dwmthumb-probe.c), under Xvfb in the shell's virtual desktop.
#
#   WINE=/opt/wine-sg/bin/wine test/dwmthumb-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dwmapi_main.c; build with x86_64_CFLAGS="-g -O2 -DSG_MUTANT_X" i386_CFLAGS=...):
# SG_MUTANT_NO_OWNER_CHECK (fails 2), SG_MUTANT_NO_OPACITY (4), SG_MUTANT_NO_RENDER (16),
# SG_MUTANT_NO_ERASE (6). 6121: SG_MUTANT_THUMB_IDLE_SECOND (live-after-quiet, live-right).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${DWMTHUMB_DPY:-247}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dwmthumb.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

for a in 64:x86_64 32:i686; do
    bits=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    TMPDIR=/var/tmp "$cc" -O2 -o "$T/dwmthumb$bits.exe" "$HERE/dwmthumb-probe.c" -ldwmapi -luser32 -lgdi32 ||
        { echo "FAIL  the $bits-bit probe did not build"; exit 1; }
done

Xvfb ":$DPY" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# the shell's virtual desktop, as a session has it (and a refresh rate)
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1

EXPECT="setup register not-before-update visible-inside outside-untouched outside-untouched-2 live-left live-right live-after-quiet
source-rect opacity-half opacity-stable hidden shown-again unregister gone-after-unregister stays-gone
other-process-dest desktop-dest remote-source"
for bits in 64 32; do
    out=$(timeout -s KILL 240 "$WINE" "$T/dwmthumb$bits.exe" 2>/dev/null | tr -d '\r')
    for name in $EXPECT; do
        line=$(printf '%s\n' "$out" | grep "^$name=" | head -1)
        case "$line" in
            "$name=ok"*) pass "$bits-bit $name ${line#*=ok }" ;;
            "") fail "$bits-bit $name: no result" ;;
            *) fail "$bits-bit $line" ;;
        esac
    done
done

[ $RC = 0 ] && echo "dwmthumb-gate: all passed" || echo "dwmthumb-gate: FAILURES"
exit $RC
