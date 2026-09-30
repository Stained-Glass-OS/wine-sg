#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A big folder shows at once (patches/sg/0595). The view sorted its items by
# reading two items' details afresh at every comparison -- tens of thousands
# of reads: File Explorer took four seconds on a folder of 2000 files here,
# longer on the QA VM, and on a network share David saw the items appear one
# by one. Now each item's text is read once for a sort. File Explorer on a
# folder of 2000 files must show them all within 2 s (it took ~4 s; ~0.8 s now).
#
#   WINE=/opt/wine-sg/bin/wine test/folderspeed-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
LIMIT="${LIMIT:-2000}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-folderspeed.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/folderspeed-probe.exe" "$HERE/folderspeed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/big"
i=0; while [ $i -lt 2000 ]; do : > "$WINEPREFIX/drive_c/big/doc$i.txt"; i=$((i + 1)); done
cp "$T/folderspeed-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
best=
for n in 1 2; do
    # to a file: the explorer the probe starts keeps a pipe open
    timeout 90 "$WINE" 'C:\folderspeed-probe.exe' 2000 'explorer.exe C:\big' > "$T/out" 2>/dev/null </dev/null
    ms=$(tr -d '\r' < "$T/out" | sed -n 's/^shown=//p')
    "$WINE" taskkill /f /im explorer.exe >/dev/null 2>&1; sleep 1
    echo "      run $n: ${ms:-none} ms"
    case "$ms" in ''|*[!0-9]*) continue ;; esac
    [ -z "$best" ] || [ "$ms" -lt "$best" ] && best=$ms
done
[ -n "$best" ] && [ "$best" -lt "$LIMIT" ] && { echo "PASS  File Explorer shows 2000 files in $best ms (< $LIMIT)"; echo "RESULT: PASS"; exit 0; }
echo "FAIL  File Explorer took ${best:-forever} ms for 2000 files (limit $LIMIT)"; echo "RESULT: FAIL"; exit 1
