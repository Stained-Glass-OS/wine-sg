#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Device interfaces through the configuration manager (patches/sg/1697),
# 64- and 32-bit: test/cmifaces-probe.c makes a root device with interfaces
# of three classes. CM_Get_Device_Interface_List (and _Size), the interface
# aliases, CM_Register/Unregister_Device_Interface and SetupDiOpenDevice-
# Interface were stubs; the interface property calls were missing or
# CR_CALL_NOT_IMPLEMENTED (HID, audio and USB tools find devices this way).
#
#   WINE=/opt/wine-sg/bin/wine test/cmifaces-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (setupapi/devinst.c): SG_MUTANT_IFACE_ALL_LISTED,
# SG_MUTANT_NO_IFACE_INSTANCE, SG_MUTANT_IGNORE_REFSTR.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cmifaces.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/cmifaces-probe.c" -lsetupapi -lcfgmgr32 -ladvapi32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/cmifaces-probe.c" -lsetupapi -lcfgmgr32 -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
