#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# OpenMP schedule(dynamic) loops as MSVC compiles them (patches/sg/6547). MSVC passes 0x20 besides
# the schedule kind (0x62 for digiKam's libraw); vcomp took that as an unknown kind and gave guided
# chunks to a loop compiled for chunks of one iteration: only each chunk's first row ran, and
# digiKam's editor showed RAW photos as a few coloured lines. test/vcompsched-probe.c runs such a
# loop on vcomp140 with 1-8 threads, 64- and 32-bit: every row once.
#   WINE=/opt/wine-sg/bin/wine test/vcompsched-gate.sh
# Mutant: SG_MUTANT_VCOMP_TYPE_WITH_MODIFIERS (dlls/vcomp/main.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vcompsched.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/vcompsched-probe.c" || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    for n in 1 2 4 8; do
        if printf '%s\n' "$out" | grep -qx "rows_$n=1"; then pass "$a: $n thread(s): every row of a 0x62 dynamic loop ran once"
        else fail "$a: $n thread(s): every row of a 0x62 dynamic loop ran once"; fi
    done
    printf '%s\n' "$out" | grep -qx "done=1" && pass "$a: the probe ran to the end" || fail "$a: the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
