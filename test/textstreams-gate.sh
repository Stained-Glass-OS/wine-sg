#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Text streams of Scripting.FileSystemObject (patches/sg/1648), 64- and
# 32-bit cscript: test/textstreams.vbs writes and reads a file following
# Line and Column, AtEndOfLine, Skip, SkipLine and WriteBlankLines, reads a
# program's output through WshShell.Exec's StdOut (ReadLine, AtEndOfStream,
# ReadAll), and uses GetStandardStream for the standard output and input.
# These were E_NOTIMPL (a pipe stream could only be written).
#
#   WINE=/opt/wine-sg/bin/wine test/textstreams-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_PIPE_EOF_EARLY, SG_MUTANT_NO_POSITION (scrrun/filesystem.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-textstreams.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
cp "$HERE/textstreams.vbs" "$T/"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for arch in "" syswow64; do
    if [ -n "$arch" ]; then cs='C:\windows\syswow64\cscript.exe'; echo "== 32-bit"; else cs=cscript; echo "== 64-bit"; fi
    out=$(cd "$T" && printf 'piped input\r\nmore\r\n' | timeout -s KILL 180 env DISPLAY= "$WINE" "$cs" //nologo 'Z:'"$(printf '%s' "$T/textstreams.vbs" | tr / '\\')" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    printf '%s\n' "$out" | grep -qx 'standard output line' && echo "PASS  GetStandardStream(1) writes to the standard output" || { echo "FAIL  nothing on the standard output"; RC=1; }
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
