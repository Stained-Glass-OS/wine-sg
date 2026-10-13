#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# amstream: the DirectDraw and audio streams' IMediaStream::AllocateSample makes
# a sample with a backing object of its own (a surface; an IAudioData with one
# second of buffer in the stream's format) and CreateSharedSample makes one
# sharing another sample's surface or IAudioData (MS_E_INCOMPATIBLE for the
# other kind of sample).
#
#   WINE=/opt/wine-sg/bin/wine test/amstream-alloc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_AM_ALLOC_NOTIMPL (amstream/ddrawstream.c), SG_MUTANT_AM_SHARED_OWN_DATA (amstream/audiostream.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-amstream-alloc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/amstream-alloc-probe.c" -lole32 -luuid &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/amstream-alloc-probe.c" -lole32 -luuid ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in amstream; do
        if command -v xvfb-run >/dev/null; then X="xvfb-run -a -s -screen\ 0\ 1024x768x24"; else X=""; fi
        out=$(cd "$T" && eval timeout -s KILL 120 $X '"$WINE"' '"$T/$p.exe"' $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
