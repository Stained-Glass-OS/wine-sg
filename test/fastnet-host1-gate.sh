#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DECISIONS-2 NET-HOST: ICLRRuntimeHost::SetHostControl/GetCLRControl (before and after Start), UnloadAppDomain and
# ICorRuntimeHost::UnloadDomain on real application domains. Needs Wine Mono (SKIP without), 64/32 bit.
#
#   WINE=/opt/wine-sg/bin/wine test/fastnet-host1-gate.sh      (ARCHS="x86_64" for one architecture)
# Mutants (mscoree/corruntimehost.c): SG_MUTANT_HOST_NOUNLOAD, _STARTED, _REFS, _CTRL.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
MONO=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] && ls "$d"/wine-mono-* >/dev/null 2>&1 && MONO=$d
done
[ -n "$MONO" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
ARCHS="${ARCHS:-x86_64 i686}"
for a in $ARCHS; do
    command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-nethost1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastnet-host1-probe.c" -lole32 -loleaut32 -luuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in $ARCHS; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
