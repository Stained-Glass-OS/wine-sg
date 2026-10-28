#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ValueSet and PropertySet made (patches/sg/6176: wintypes, where they are registered, activated them as
# a stub; windows.applicationmodel's property set makes them now), and ResourceContext as a class with
# its statics (GetForViewIndependentUse, SetGlobalQualifierValue) -- Windows Terminal's connection
# settings and resources. test/valueset-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/valueset-gate.sh
# Mutants (dlls/windows.applicationmodel): SG_MUTANT_VALUESET_NOT_MADE (property_set.c),
# SG_MUTANT_RESOURCE_CONTEXT_NO_CLASS (resource_manager.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-valueset.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/valueset-probe.c" -lruntimeobject -luuid || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'Windows.Foundation.Collections.ValueSet=00000000,1,0,1,1,00000000,8000000b' "a ValueSet: made, its map keeps a value (Lookup of a missing key: E_BOUNDS)"
chk 'Windows.Foundation.Collections.PropertySet=00000000,1,0,1,1,00000000,8000000b' "a PropertySet: the same"
chk 'context=00000000,1,1,1,00000000,00000000' "ResourceContext: made, cloned; GetForViewIndependentUse; global qualifier values taken"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
