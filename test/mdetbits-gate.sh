#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IMediaDet::GetBitmapBits takes a buffer without a size pointer (patches/sg/9417).
#
# Microsoft's poster-frame sample asks for the size, then for the bits with no
# size pointer (GetBitmapBits(t, NULL, buffer, w, h)); Wine refused that call
# with E_POINTER and thumbnailers built on the sample drew nothing.
#
# The probe (test/mdetbits-probe.c) writes an RGB24 AVI whose every frame has a
# colour of its own and asks MediaDet for the frame at 1 s both ways.
#   WINE=/opt/wine-sg/bin/wine test/mdetbits-gate.sh
# Mutant: SG_MUTANT_MDET_SIZE_REQUIRED (qedit mediadet.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-mdetbits.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/mdetbits-probe.c" -lole32 -loleaut32 -luuid -lstrmiids \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
# to a file: services started with the prefix keep a pipe open
timeout -s KILL 120 "$WINE" 'C:\probe.exe' >"$T/out" 2>/dev/null
out=$(tr -d '\r' < "$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check write   "the probe wrote its AVI"
check open    "MediaDet opens it and finds the video stream"
check size    "GetBitmapBits with no buffer reports the size"
check nosize  "GetBitmapBits with a buffer and no size pointer hands out the frame at 1 s"
check neither "neither pointer is refused (E_POINTER)"
exit $RC
