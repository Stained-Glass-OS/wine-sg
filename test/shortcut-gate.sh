#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Create shortcut (patches/sg/0596). The shell menu's Create shortcut did
# nothing (David: "Create link does not do anything in the SMB share"). A
# shortcut is made beside the file; where the folder cannot be written (a
# read-only share), on the desktop if the user agrees.
#
#   WINE=/opt/wine-sg/bin/wine test/shortcut-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ "$(id -u)" != 0 ] || { echo "SKIP: root writes anywhere"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shortcut.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/shortcut-probe.exe" "$HERE/shortcut-probe.c" -lshell32 -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/shortcut-probe.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/readonly"; : > "$WINEPREFIX/drive_c/readonly/file.txt"; chmod 555 "$WINEPREFIX/drive_c/readonly"
cd "$WINEPREFIX/drive_c"
rc=0
for m in here readonly; do
    timeout 60 "$WINE" 'C:\shortcut-probe.exe' $m > "$T/out" 2>/dev/null; r=$?
    echo "      $(tr -d '\r' < "$T/out")"
    case "$m:$r" in
    here:0) echo "PASS  Create shortcut makes 'file.txt - Shortcut.lnk' beside the file" ;;
    readonly:0) echo "PASS  where it cannot, it asks and puts the shortcut on the desktop" ;;
    *) echo "FAIL  $m ($r)"; rc=1 ;;
    esac
done
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
