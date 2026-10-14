#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# tbs.dll (patches/sg/5517): without a TPM, Tbsi_Context_Create and
# Tbsi_GetDeviceInfo validate their arguments as documented before
# answering TBS_E_TPM_NOT_FOUND. 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/tbs-gate.sh
# Mutant (tbs.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_TBS_NOCHECK"):
# no argument checks.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-tbs.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
x86_64-w64-mingw32-gcc -O2 -o "$T/tb64.exe" "$HERE/tbs-probe.c" || { echo "FAIL  probe"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/tb32.exe" "$HERE/tbs-probe.c" || { echo "FAIL  probe"; exit 1; }
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

for b in 64 32; do
    out=$(DISPLAY= timeout -s KILL 120 "$WINE" "$T/tb$b.exe" 2>/dev/null | tr -d '\r')
    n_ok=$(printf '%s\n' "$out" | grep -c '^ok ')
    printf '%s\n' "$out" | grep '^FAIL' | while read -r line; do printf 'FAIL  %s-bit %s\n' "$b" "$line"; done
    if printf '%s\n' "$out" | grep -q '^FAIL' || ! printf '%s\n' "$out" | grep -qx 'tbs-probe: all ok'; then
        RC=1; printf 'FAIL  %s-bit probe (%s checks ok)\n' "$b" "$n_ok"
    else
        printf 'PASS  %s-bit %s checks\n' "$b" "$n_ok"
    fi
done

[ $RC = 0 ] && echo "tbs-gate: all passed" || echo "tbs-gate: FAILURES"
exit $RC
