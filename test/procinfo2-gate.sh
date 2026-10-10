#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# NtQueryInformationProcess ProcessIoPriority, ProcessProtectionInformation and
# ProcessCommandLineInformation (patches/sg/2238). Each probe also reads the
# command line of a child of the other bitness (a 32-bit probe reading a
# 64-bit child goes through wow64).
#
#   WINE=/opt/wine-sg/bin/wine test/procinfo2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_IOPRIO, SG_MUTANT_CMDLINE_OWN (ntdll/unix/process.c).
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
T=$(mktemp -d /var/tmp/sg-procinfo2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:229
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { pkill -9 -f "$T/probe-" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/procinfo2-probe.c" -lkernel32 -lntdll \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY="$DISP" "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    o=$([ $a = x86_64 ] && echo i686 || echo x86_64)
    echo "== $a (other process: $o)"
    out=$(cd "$T" && timeout -s KILL 240 env DISPLAY="$DISP" "$WINE" "$T/probe-$a.exe" "$T/probe-$o.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
