#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The EVR filter with a presenter of the program's own (patches/sg/6532), as MPC-HC's EVR custom
# presenter uses it: GetService(MR_VIDEO_RENDER_SERVICE) gives the renderer's IMFVideoRenderer, a
# mixer given three streams mixes as soon as the reference stream has a frame, and the presenter
# finds an IMFClock and hears OnClockStart. The probe plays an AVI it writes through a DirectShow
# graph and reads the frames back: red, changing, not black (test/evrpresenter-probe.c; its own
# X server).
#   WINE=/opt/wine-sg/bin/wine test/evrpresenter-gate.sh
# Mutants (dlls/evr): SG_MUTANT_EVR_RENDERER_SERVICE (evr.c), SG_MUTANT_EVR_ALL_INPUTS (mixer.c),
# SG_MUTANT_EVR_NO_CLOCK_STATE (evr.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-evrpresenter.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/evrpresenter-probe.c" -lole32 -luuid -lstrmiids -ld3d9 -ldxva2 -levr \
    -lmfplat -lmfuuid -luser32 || { fail "probe did not build"; exit 1; }
if [ -n "${SG_EVR_DISPLAY:-}" ]; then
    Xvfb ":$SG_EVR_DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -e "/tmp/.X11-unix/X$SG_EVR_DISPLAY" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    echo "$SG_EVR_DISPLAY" > "$T/display"
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check device           "a Direct3D 9 device and its device manager"
check three_streams    "the EVR configured with three streams"
check renderer_service "GetService(MR_VIDEO_RENDER_SERVICE) gives the renderer's IMFVideoRenderer"
check own_presenter    "the program's presenter installed"
check rendered         "the AVI rendered to the EVR"
check clock            "the presenter finds the graph's clock (running, its time moving on)"
check clock_start      "the presenter hears OnClockStart"
check frames           "the mixer gives frames while the substreams have none (20+ in 1.5 s)"
check picture          "the frames are the video's red, not black"
check moving           "and they change from frame to frame"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
