#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# t2embed: fonts embedded in documents (the EOT structure) and loaded back:
# TTEmbedFont, TTEmbedFontFromFileA, TTGetEmbeddedFontInfo, TTLoadEmbeddedFont, TTDeleteEmbeddedFont (6009)
#   WINE=/opt/wine-sg/bin/wine test/uspfeat-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${EOT_DPY:-531}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-eot.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/eot-probe.exe" "$HERE/eot-probe.c" -lt2embed -lgdi32 2>/dev/null && built=1
/usr/bin/python3 -I "$HERE/uspfeat-font.py" "$T/sgfeat.ttf" &&
    /usr/bin/python3 -I "$HERE/uspfeat-font.py" "$T/sglocked.ttf" 2 "SG Locked Font" || { echo "FAIL  the test fonts were not made"; exit 1; }
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/eot-probe.exe" "Z:$T/sgfeat.ttf" "Z:$T/sglocked.ttf" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1

[ $rc = 0 ] || { grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"; }
exit $rc
