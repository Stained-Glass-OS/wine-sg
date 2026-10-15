#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# kernelapi IsDestinationReachable, connect IsInternetConnected (patch 3741)
#   WINE=/opt/wine-sg/bin/wine test/fastb-kernel-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${FASTBMS_DPY:-437}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-fastb-kernel.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

OBJ=$(CDPATH='' cd -- "$(dirname -- "$WINE")" && pwd)
SRC=$(CDPATH='' cd -- "$OBJ/.." && pwd)
[ -x "$OBJ/tools/winegcc/winegcc" ] && [ -f "$SRC/include/ddk/wdm.h" ] || { echo "SKIP: no build tree beside $WINE"; exit 77; }
for lib in dlls/winecrt0 dlls/ntoskrnl.exe dlls/hal; do
    ls "$OBJ/$lib/x86_64-windows/"lib*.a >/dev/null 2>&1 || { echo "SKIP: $lib is not built"; exit 77; }
done
CF="-I$HERE -I$OBJ/include -I$SRC/include -I$SRC/include/msvcrt -D_MSVCR_VER=0 -D__WINESRC__ -D__WINE_PE_BUILD -fno-strict-aliasing -Wno-packed-not-aligned -O1"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -c -o "$T/drv.o" $CF "$HERE/fastb-kdrv.c" || { echo "FAIL  the driver did not compile"; exit 1; }
TMPDIR=/var/tmp "$OBJ/tools/winegcc/winegcc" -o "$T/fastb-kdrv.sys" --wine-objdir "$OBJ" -b x86_64-w64-mingw32 -s -nodefaultlibs -nostartfiles \
    -Wl,--subsystem,native -shared "$T/drv.o" "$OBJ/dlls/winecrt0/x86_64-windows/libwinecrt0.a" \
    "$OBJ/dlls/ntoskrnl.exe/x86_64-windows/libntoskrnl.a" "$OBJ/dlls/hal/x86_64-windows/libhal.a" || { echo "FAIL  the driver did not link"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -I"$HERE" -o "$T/fastb-kernel-probe.exe" "$HERE/fastb-kernel-probe.c" -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
DRV="Z:$(printf '%s' "$T/fastb-kdrv.sys" | tr / '\\')"
cd "$T" && timeout -s KILL 240 "$WINE" "$T/fastb-kernel-probe.exe" "$DRV" >"$T/out.raw" 2>"$T/err" </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"; [ -n "${GATE_ERR_COPY:-}" ] && cp "$T/err" "$GATE_ERR_COPY"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
