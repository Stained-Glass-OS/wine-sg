#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A format check on the connected EVR input pin keeps the presenter's services (patches/sg/6548).
# The EVR filter brought its services up for every QueryAccept and released them after it; asked
# while streaming (LAV Video's dynamic format change with 4:4:4 H.264), that released the
# presenter's service pointers -- MPC-HC's presenter stops its threads there, and the picture
# stayed black. test/evrqueryaccept-probe.c: a counting presenter (wrapping MFCreateVideoPresenter's)
# on the EVR, an uncompressed RGB32 AVI rendered into it, QueryAccept on the connected pin.
#   WINE=/opt/wine-sg/bin/wine test/evrqueryaccept-gate.sh
# Mutant: SG_MUTANT_EVR_QUERYACCEPT_RELEASES (dlls/evr/evr.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb ffmpeg "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-evrqa.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/evrqueryaccept-probe.c" -lole32 -lstrmiids -levr -luuid ||
    { fail "probe did not build"; exit 1; }
ffmpeg -v error -y -f lavfi -i testsrc=size=64x48:rate=5 -t 2 -c:v rawvideo -pix_fmt bgra "$T/rgb.avi" ||
    { fail "test video was not made"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$T/rgb.avi" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 'C:\rgb.avi' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
printf '%s\n' "$out" | grep -q "connected=1" && pass "an RGB32 AVI renders into the EVR with a custom presenter" ||
    fail "an RGB32 AVI renders into the EVR with a custom presenter"
check accepted      "QueryAccept of the connection's own type succeeds"
check services_kept "the presenter keeps its services (no ReleaseServicePointers)"
check not_reinited  "and gets no second InitServicePointers"
check done          "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
