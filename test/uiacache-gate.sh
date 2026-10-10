#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UiaGetUpdatedCache with the children, descendants and subtree scopes (patches/sg/2626):
# test/uiacache-probe.c, under Xvfb, windows walked by UiaNavigate and by the cache request.
#
#   WINE=/opt/wine-sg/bin/wine test/uiacache-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (uiautomationcore, uia_client.c): SG_MUTANT_UIA_CACHE_NO_CHILDREN (the children are left out),
# SG_MUTANT_UIA_CACHE_FLAT_TREE (descendants stay at the first level).
# The probe needs the uiautomation headers with UiaGetUpdatedCache: Wine's own are used when the system ones lack them.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${UIACACHE_DPY:-246}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-uiacache.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
for inc in "" "-I$(dirname "$WINE")/include" "-I$HERE/../../obj/include" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -mwindows $inc -o "$T/uiacache-probe.exe" "$HERE/uiacache-probe.c" \
        -luiautomationcore -lole32 -loleaut32 -luuid -luser32 2>/dev/null && { built=1; break; }
done
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/uiacache-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
