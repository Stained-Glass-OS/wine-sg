#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Scripting.FileSystemObject's File, Folder and Drive objects
# (patches/sg/1649), 64- and 32-bit cscript: test/fsoitems.vbs reads a
# folder's size, type, dates, attributes, parent, drive and short names,
# adds and finds subfolders and files by name, renames, copies, moves and
# deletes files and folders through their objects, moves files by
# wildcard, moves a folder, and reads drives. These were E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/fsoitems-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_MOVEFOLDER (scrrun/filesystem.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fsoitems.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
cp "$HERE/fsoitems.vbs" "$T/"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for arch in "" syswow64; do
    if [ -n "$arch" ]; then cs='C:\windows\syswow64\cscript.exe'; echo "== 32-bit"; else cs=cscript; echo "== 64-bit"; fi
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$cs" //nologo 'Z:'"$(printf '%s' "$T/fsoitems.vbs" | tr / '\\')" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
