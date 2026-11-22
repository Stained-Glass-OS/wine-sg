#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wined3d (GL): a context drops a released swapchain (patches/sg/8293) and its backup window gets a
# pixel format when the window's own was taken by the program's OpenGL (8294): test/d3d9ctxswap-probe.c,
# under Xvfb, with warn+heap (freed memory filled) and the wined3d errors checked.  Sweet Home 3D.
#
#   WINE=/opt/wine-sg/bin/wine test/d3d9ctxswap-gate.sh
# Mutants: SG_MUTANT_CONTEXT_KEEPS_SWAPCHAIN (dlls/wined3d/swapchain.c),
# SG_MUTANT_BACKUP_KEEPS_FORMAT_FLAG (dlls/wined3d/context_gl.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${D3D9CTXSWAP_DPY:-286}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d3d9ctxswap.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
for inc in "" "-I$(dirname "$WINE")/include" "-I$HERE/../../obj/include" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -mwindows $inc -o "$T/d3d9ctxswap-probe.exe" "$HERE/d3d9ctxswap-probe.c" \
        -luser32 -ld3d9 -lgdi32 -lopengl32 2>/dev/null && { built=1; break; }
done
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+d3d,warn+heap} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;d3d9=b" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/d3d9ctxswap-probe.exe" >"$T/out.raw" 2>"$T/err" </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
for e in "Failed to get a device context for window" "Fallback to backup window" "Failed to map bo"; do
    n=$(grep -c "$e" "$T/err")
    if [ "$n" = 0 ]; then echo "PASS  wined3d: no \"$e\""; else echo "FAIL  wined3d: $n x \"$e\""; grep -m2 "$e" "$T/err"; fi >> "$T/out"
done
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
