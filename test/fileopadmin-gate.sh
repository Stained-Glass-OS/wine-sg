#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder the user may not write to asks for an administrator
# (patches/sg/0589). A paste there did nothing and said nothing (David: "if
# you copy to a protected place on the C: it should ask for admin"); Windows
# asks "You'll need to provide administrator permission to copy to this
# folder" and does it elevated on Continue. Here: the question comes, Cancel
# cancels; with FOF_NOERRORUI the copy fails instead of returning 0; and a
# paste of files put on the clipboard without a drop effect copies them (it
# did nothing). The elevated copy itself needs the session's elevation
# broker: QA VM.
#
#   WINE=/opt/wine-sg/bin/wine test/fileopadmin-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ "$(id -u)" != 0 ] || { echo "SKIP: root writes anywhere"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fileopadmin.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/fileopadmin-probe.exe" "$HERE/fileopadmin-probe.c" -lshell32 -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/fileopadmin-probe.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/readonly"; chmod 555 "$WINEPREFIX/drive_c/readonly"
cd "$WINEPREFIX/drive_c"
rc=0
for m in denied noerrorui paste; do
    timeout 60 "$WINE" 'C:\fileopadmin-probe.exe' $m > "$T/out" 2>/dev/null; r=$?
    out=$(tr -d '\r' < "$T/out")
    echo "      $out"
    case "$m:$r" in
    denied:0) echo "PASS  a copy into a folder the user may not write to asks for an administrator" ;;
    noerrorui:0) echo "PASS  without error UI the copy fails, not 0" ;;
    paste:0) echo "PASS  files on the clipboard without a drop effect are pasted" ;;
    *) echo "FAIL  $m ($r)"; rc=1 ;;
    esac
done
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
