#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll (5923): the hidden/system attributes (a user extended attribute) of a read-only file are kept,
# set and created alike (they were lost: the attribute was written after the write permission went).
#
#   WINE=/opt/wine-sg/bin/wine test/rohidden-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_RO_XATTR_AFTER_CHMOD (dlls/ntdll/unix/file.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rohidden.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/rohidden-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
probe_xattr() { touch "$T/x" && setfattr -n user.sgtest -v 1 "$T/x" 2>/dev/null; }
command -v setfattr >/dev/null && ! probe_xattr && { echo "SKIP: $T has no user extended attributes"; exit 77; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\r') > "$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && exit 0
exit 1
