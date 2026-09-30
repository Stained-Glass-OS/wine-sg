#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's own right-click verbs show their names (patches/sg/0591).
# shell32 read a verb's default value with a size query that answers
# ERROR_SUCCESS, not ERROR_MORE_DATA, and fell back to the key's name: "Run
# with debugging" showed as "sgdebugreport" in File Explorer (David could not
# find it). A verb named by its default value and one named by MUIVerb.
#
#   WINE=/opt/wine-sg/bin/wine test/verblabel-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-verblabel.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/verblabel-probe.exe" "$HERE/verblabel-probe.c" -lshell32 -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/verblabel-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout 60 "$WINE" 'C:\verblabel-probe.exe' > "$T/out" 2>/dev/null; r=$?
sed 's/^/      /' "$T/out" | tr -d '\r' | grep -v '^      item: $'
[ $r = 0 ] && { echo "PASS  a verb's menu item shows its name (default value, MUIVerb), not its key"; echo "RESULT: PASS"; exit 0; }
echo "FAIL  verbs show their keys' names"; echo "RESULT: FAIL"; exit 1
