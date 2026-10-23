#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winmm mmioOpen MMIO_GETTEMP (6066, C3): refused (MMIOERR_CANNOTOPEN from the DOS I/O procedure). Now, as
# documented, it makes a temporary file name into the caller's 128-character buffer and returns
# MMSYSERR_NOERROR (0): in the temporary directory for an empty name, in the directory the name names or the
# directory of the file it names, unique each time; no buffer gives MMIOERR_FILENOTFOUND; mmioOpenW too.
#   WINE=/opt/wine-sg/bin/wine test/mmiotemp-gate.sh   (mutant SG_MUTANT_WINMM_GETTEMP_TEMPDIR_ONLY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mmiotemp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/mmiotemp-probe.exe" "$HERE/mmiotemp-probe.c" -lwinmm || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/mmiotemp-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" mmiotemp-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Empty "0 1 1" "an empty name: a temporary file in the temporary directory, MMSYSERR_NOERROR"
expect Unique 1 "a new name each time"
expect InDir "0 1" "a file's name: one in its directory (the flag overriding MMIO_READ)"
expect Dir "0 1" "a directory's name: one in it"
expect Null "257 257" "no buffer: MMIOERR_FILENOTFOUND"
expect Wide "0 1" "mmioOpenW too"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
