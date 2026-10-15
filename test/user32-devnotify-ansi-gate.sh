#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE: ANSI window device events: DBT_DEVTYP_PORT W to A, others unchanged (5210)
#
#   WINE=/opt/wine-sg/bin/wine test/user32-devnotify-ansi-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree); on a build tree the gate
#   needs a display (DISPLAY, e.g. Xvfb) because the probe creates windows or screen DCs.
# Mutants: DEVA_NOPORT
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-user32-devnotify-ansi.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
[ -n "${SG_GATE_DISPLAY:-}" ] && export DISPLAY="$SG_GATE_DISPLAY"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
# the plug-and-play RPC client stubs: widl and the interface description from a Wine build tree
WIDL="${WIDL:-$(dirname "$WINE")/tools/widl/widl}"
IDL="${PLUGPLAY_IDL:-$(dirname "$WINE")/../wine-10.0/include/wine/plugplay.idl}"
[ -x "$WIDL" ] && [ -f "$IDL" ] || { echo "SKIP: no widl ($WIDL) or plugplay.idl ($IDL)"; exit 77; }
INC=$(dirname "$IDL")/..
for m in 64 32; do
    mkdir "$T/w$m" && (cd "$T/w$m" && "$WIDL" -m$m -I"$INC" -I"$INC/wine" -c -h "$IDL" >/dev/null 2>&1) ||
        { echo "FAIL  widl did not build the stubs"; exit 1; }
done
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -I"$T/w64" -o "$T/p64.exe" "$HERE/user32-devnotify-ansi-probe.c" "$T/w64/plugplay_c.c" -luser32 -lrpcrt4 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -I"$T/w32" -o "$T/p32.exe" "$HERE/user32-devnotify-ansi-probe.c" "$T/w32/plugplay_c.c" -luser32 -lrpcrt4 ||
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
