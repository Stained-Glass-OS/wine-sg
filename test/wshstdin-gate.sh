#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WScript.StdIn is one stream (patches/sg/9623): cscript runs test/wshstdin.vbs
# with its input redirected from a file and reads it with the AtEndOfStream /
# ReadLine loop, then feeds a child script (test/wshstdin-child.vbs) through
# WshShell.Exec's StdIn. Headless.
#
#   WINE=/opt/wine-sg/bin/wine test/wshstdin-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (programs/wscript/host.c): SG_MUTANT_WSH_STD_NEW_EACH_TIME.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wshstdin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/wshstdin.vbs" "$HERE/wshstdin-child.vbs" "$C/"
printf 'one\r\ntwo\r\nthree\r\n' > "$T/input.txt"
(cd "$C" && timeout -s KILL 180 "$WINE" cscript.exe //nologo wshstdin.vbs < "$T/input.txt" 2>&1 | tr -d '\r' > "$T/probe.out")
cat "$T/probe.out"
if [ "$(grep -c '^PASS' "$T/probe.out")" = 3 ] && ! grep -q '^FAIL' "$T/probe.out"; then
    echo "RESULT: PASS"; exit 0
fi
echo "RESULT: FAIL"
exit 1
