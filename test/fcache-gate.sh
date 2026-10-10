#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GetSystemFileCacheSize / SetSystemFileCacheSize, SystemFileCacheInformation and
# Get/SetThreadDpiHostingBehavior (patches/sg/2208): test/fcache-probe.c.  The
# limits come from the Linux page cache and, once set, are read back by every
# process (a 64- and a 32-bit one are started by each probe).
#
#   WINE=/opt/wine-sg/bin/wine test/fcache-gate.sh
# Mutants: SG_MUTANT_FCACHE_NO_STORE (kernelbase/memory.c), SG_MUTANT_DPI_HOST_GLOBAL
# (user32/sysparams.c), SG_MUTANT_FCACHE_ZERO (ntdll/unix/system.c), SG_MUTANT_FCACHE_NO_PRIV
# (kernelbase/memory.c).
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
T=$(mktemp -d /var/tmp/sg-fcache.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:228
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { pkill -9 -f "$T/probe-" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fcache-probe.c" -luser32 -ladvapi32 -lkernel32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY="$DISP" "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    o=$([ $a = x86_64 ] && echo i686 || echo x86_64)
    echo "== $a (other process: $o)"
    # the limits are volatile: a fresh wineserver starts at the defaults again
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
    out=$(cd "$T" && timeout -s KILL 240 env DISPLAY="$DISP" "$WINE" "$T/probe-$a.exe" "$T/probe-$o.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
