#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE: WINNLSEnableIME previous state and WINNLSGetEnableStatus (5213)
#
#   WINE=/opt/wine-sg/bin/wine test/user32-winnls-ime-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree); on a build tree the gate
#   needs a display (DISPLAY, e.g. Xvfb) because the probe creates windows or screen DCs.
# Mutants: NLS_ALWAYS_TRUE
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-user32-winnls-ime.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
[ -n "${SG_GATE_DISPLAY:-}" ] && export DISPLAY="$SG_GATE_DISPLAY"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/user32-winnls-ime-probe.c" -luser32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/user32-winnls-ime-probe.c" -luser32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
