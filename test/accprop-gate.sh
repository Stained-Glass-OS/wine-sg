#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IAccPropServices (patches/sg/2602): test/accprop-probe.c. Every method past
# QueryInterface returned E_NOTIMPL/did nothing; now a real process-wide
# property/server store backs Set*/Clear*/Compose*/Decompose*.
#
#   WINE=/opt/wine-sg/bin/wine test/accprop-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_ACCPROP_NO_RELEASE_OLD (oleacc/propservice.c) -- re-registering
# a server for the same identity+property leaks the old registration instead
# of releasing it.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-accprop.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/accprop-probe.exe" "$HERE/accprop-probe.c" \
    -loleacc -lole32 -loleaut32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 60 "$WINE" "$T/accprop-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
