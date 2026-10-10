#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RtlQueryRegistryValues (SUBKEY, NOVALUE with a name, REQUIRED with a default) and
# NtOpenKey with no access still opening (patches/sg/2213): test/rtlquery-probe.c.  Uses a key of its
# own under HKLM\Software; no child process.
#
#   WINE=/opt/wine-sg/bin/wine test/rtlquery-gate.sh
# Mutants: SG_MUTANT_SUBKEY_OLD, SG_MUTANT_REQUIRED_ALWAYS (ntdll/reg.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rtlquery.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:229
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { pkill -9 -f "$T/probe-" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/rtlquery-probe.c" -ladvapi32 -lkernel32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY="$DISP" "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    o=$([ $a = x86_64 ] && echo i686 || echo x86_64)
    echo "== $a (other process: $o)"
    out=$(cd "$T" && timeout -s KILL 240 env DISPLAY="$DISP" "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
