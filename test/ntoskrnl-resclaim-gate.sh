#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE ntoskrnl: IoReportResourceForDetection (claiming and releasing hardware resources, conflicts).
#
#   WINE=/opt/wine-sg/bin/wine test/ntoskrnl-resclaim-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_RES_NO_CONFLICT, SG_MUTANT_RES_SHARED (ntoskrnl.exe/ntoskrnl.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ntoskrnl-resclaim.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/ntoskrnl-resclaim-probe.c" -ladvapi32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/ntoskrnl-resclaim-probe.c" -ladvapi32 ||
    { echo "FAIL  probe did not build"; exit 1; }
# the probe driver, next to the host that loads it
DDK64=; DDK32=
for d in /usr/x86_64-w64-mingw32/include/ddk /usr/share/mingw-w64/include/ddk; do [ -d "$d" ] && DDK64=$d && break; done
for d in /usr/i686-w64-mingw32/include/ddk /usr/share/mingw-w64/include/ddk; do [ -d "$d" ] && DDK32=$d && break; done
[ -n "$DDK64" ] && [ -n "$DDK32" ] || { echo "SKIP: no mingw DDK headers"; exit 77; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -I"$HERE" -I"$DDK64" -o "$T/sgdrv64.sys" "$HERE/ntoskrnl-resclaim-driver.c" -shared -nostdlib \
    -nostartfiles -Wl,--subsystem,native -Wl,-e,DriverEntry -lntoskrnl -lhal &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -I"$HERE" -I"$DDK32" -o "$T/sgdrv32.sys" "$HERE/ntoskrnl-resclaim-driver.c" -shared -nostdlib \
    -nostartfiles -Wl,--subsystem,native -Wl,-e,_DriverEntry@8 -lntoskrnl -lhal ||
    { echo "FAIL  probe driver did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in ntoskrnl.exe; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
