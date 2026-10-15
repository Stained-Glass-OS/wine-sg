#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE: EMF playback of the escape records (5222)
#
#   WINE=/opt/wine-sg/bin/wine test/gdi32-emf-escapes-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree); on a build tree the gate
#   needs a display (DISPLAY, e.g. Xvfb) because the probe creates windows or screen DCs.
# Mutants: EMF_ESCAPE
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-gdi32-emf-escapes.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,fixme+enhmetafile,trace+enhmetafile WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
[ -n "${SG_GATE_DISPLAY:-}" ] && export DISPLAY="$SG_GATE_DISPLAY"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/gdi32-emf-escapes-probe.c" -lgdi32 -luser32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/gdi32-emf-escapes-probe.c" -lgdi32 -luser32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/$p.exe" 2>"$T/err.txt" </dev/null | tr -d '\r')
    for want in 'ExtEscape: escape 4660, 12 bytes' 'DrawEscape: escape 85, 12 bytes' 'NamedEscape: driver L"WINEDRV", escape 102, 12 bytes'; do
        grep -qF "$want" "$T/err.txt" || { echo "FAIL $p: no trace '$want'"; RC=1; }
    done
    if grep -qF 'NamedEscape: driver L"WINEDRV", escape 102, 12 bytes' "$T/err.txt" && [ "$(grep -cF 'NamedEscape:' "$T/err.txt")" != 1 ]; then echo "FAIL $p: the damaged NamedEscape record was played"; RC=1; fi
    if [ "$(grep -cF 'ExtEscape: escape' "$T/err.txt")" != 1 ]; then echo "FAIL $p: the damaged ExtEscape record was played"; RC=1; fi
    if grep -q 'is unimplemented' "$T/err.txt"; then echo "FAIL $p: an escape record is reported unimplemented:"; grep 'is unimplemented' "$T/err.txt" | head -3; RC=1; fi
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
