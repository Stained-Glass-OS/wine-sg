#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcrt: _abnormal_termination and _heapmin, ntdll RtlCompactHeap
#   WINE=/opt/wine-sg/bin/wine test/fastb-abterm-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${FASTBMS_DPY:-448}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-fastb-abterm.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
build() {   # build ARCH-compiler OUT
    for inc in "" "-I$(dirname "$WINE")/include" "-I$HERE/../../obj/include" "-I/opt/wine-sg/include/wine/windows"; do
        TMPDIR=/var/tmp "$1" -O2 -mwindows $inc -o "$2" "$HERE/fastb-abterm-probe.c" -ladvapi32 2>/dev/null && return 0
    done
    return 1
}
build "$MINGW" "$T/probe64.exe" || { echo "FAIL  the probe did not build"; exit 1; }
HAVE32=
if command -v "$MINGW32" >/dev/null; then build "$MINGW32" "$T/probe32.exe" && HAVE32=1 || { echo "FAIL  the 32-bit probe did not build"; exit 1; }; fi

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
rc=0
for exe in probe64.exe ${HAVE32:+probe32.exe}; do
    cd "$T" && timeout -s KILL 60 "$WINE" "$T/$exe" >"$T/out.raw" 2>/dev/null </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out"
    echo "== $exe"
    cat "$T/out"
    grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
    grep -q '^RESULT:' "$T/out" || { echo "RESULT: FAIL (the probe gave no result)"; rc=1; }
done
exit $rc
