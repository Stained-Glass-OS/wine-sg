#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# olepro32.dll and dpnhpast.dll (patches/sg/5527): neither implements a class,
# so DllGetClassObject answers CLASS_E_CLASSNOTAVAILABLE and leaves a NULL
# pointer (olepro32 used to say S_OK and leave the pointer unset). 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/dllclass-gate.sh
# Mutant (dlls/olepro32/olepro32stubs.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_OLEPRO32_OK"):
# the old S_OK.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dllclass.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
x86_64-w64-mingw32-gcc -O2 -o "$T/tb64.exe" "$HERE/dllclass-probe.c" || { echo "FAIL  probe"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/tb32.exe" "$HERE/dllclass-probe.c" || { echo "FAIL  probe"; exit 1; }
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

for b in 64 32; do
    out=$(DISPLAY= timeout -s KILL 120 "$WINE" "$T/tb$b.exe" 2>/dev/null | tr -d '\r')
    n_ok=$(printf '%s\n' "$out" | grep -c '^ok ')
    printf '%s\n' "$out" | grep '^FAIL' | while read -r line; do printf 'FAIL  %s-bit %s\n' "$b" "$line"; done
    if printf '%s\n' "$out" | grep -q '^FAIL' || ! printf '%s\n' "$out" | grep -qx 'dllclass-probe: all ok'; then
        RC=1; printf 'FAIL  %s-bit probe (%s checks ok)\n' "$b" "$n_ok"
    else
        printf 'PASS  %s-bit %s checks\n' "$b" "$n_ok"
    fi
done

[ $RC = 0 ] && echo "dllclass-gate: all passed" || echo "dllclass-gate: FAILURES"
exit $RC
