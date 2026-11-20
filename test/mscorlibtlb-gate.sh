#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mscorlib.tlb (wine-sg 7743): the type library of mscorlib's COM-visible types (_Type, _AppDomain,
# IEnumerable, ...) is installed in Framework and Framework64 v4.0.30319 and registered as
# {BED7F4EA-1A96-11D2-8F08-00A0C9A6186D} 2.4. Visual Studio 2022 stops at "mscorlib.tlb could not be
# loaded" without it. test/mscorlibtlb-probe.c, 64- and 32-bit, in a fresh prefix.
#   WINE=/opt/wine-sg/bin/wine test/mscorlibtlb-gate.sh
# Mutants (manual): the wine.inf FakeDlls lines removed (not installed), and the IDL without
# "#pragma makedep regtypelib" (installed but not registered).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mscorlibtlb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/p64" "$T/p32"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64/probe.exe" "$HERE/mscorlibtlb-probe.c" -loleaut32 -lole32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32/probe.exe" "$HERE/mscorlibtlb-probe.c" -loleaut32 -lole32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T/$p" && timeout -s KILL 120 "$WINE" "$T/$p/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"; printf '%s\n' "$out" | grep '^skip\|^info' 
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
