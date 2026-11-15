#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer keeps each folder's sort (David 2026-11-15: "I keep having to sort by
# date"): a folder never sorted is by Date modified, newest first; a click on a column
# header sorts it so, and the folder keeps that when it is opened again (FolderSorts).
# Told from the screen: which of two files, a-old.txt (2020) and z-new.txt (now), is
# higher in the view (tesseract).
#
#   WINE=/opt/wine-sg/bin/wine test/explorer-sortmem-gate.sh
#   SG_FE_EXE=<explorer.exe> runs that build of File Explorer (a dev tree's)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
FE="${SG_FE_EXE:-explorer}"
DPY="${EXPLORERSORT_DPY:-156}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb xdotool import convert tesseract "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-explorersort.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/explorer-probe.exe" "$HERE/explorer-probe.c" -lole32 -lshell32 -lshlwapi -luuid -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/explorer-probe.exe" "$C/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$C/sortgate"
echo old > "$C/sortgate/a-old.txt"; touch -d '2020-01-02 03:04' "$C/sortgate/a-old.txt"
echo new > "$C/sortgate/z-new.txt"

Xvfb ":$DPY" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
P() { "$WINE" 'C:\explorer-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
"$WINE" explorer /desktop=shell,1280x800 >/dev/null 2>&1 &
sleep 6

# which file is higher on the screen: "new" (z-new above a-old), "old", or "?"
first() {
    sleep 1.5; import -window root "$T/$1.png"
    convert "$T/$1.png" -resize 200% "$T/$1-2x.png"
    tesseract "$T/$1-2x.png" - tsv 2>/dev/null | awk -F'\t' '
        tolower($12) ~ /-old/ && !a { a = $8 } tolower($12) ~ /-new/ && !z { z = $8 }
        END { if (a && z) print (z < a ? "new" : "old"); else print "?" }'
    rm -f "$T/$1-2x.png"
}
openit() {
    "$WINE" "$FE" 'C:\sortgate' >/dev/null 2>&1 &
    P wait-title sortgate 30 >/dev/null
    sleep 2
}

openit
[ "$(first never)" = new ] && pass "a folder never sorted is by Date modified, newest first" || fail "first open: $(first never2)"

# the Name column's header: where tesseract reads "Name"
convert "$T/never.png" -resize 200% "$T/never-2x.png"
set -- $(tesseract "$T/never-2x.png" - tsv 2>/dev/null | awk -F'\t' '$12 == "Name" { print ($7 + $9 / 2) / 2, ($8 + $10 / 2) / 2; exit }')
if [ $# -eq 2 ]; then
    xdotool mousemove "${1%.*}" "${2%.*}" click 1
    [ "$(first byname)" = old ] && pass "a click on Name sorts by name" || fail "after the Name click: $(first byname2)"
else
    fail "no Name column header on screen"
fi

P close-all >/dev/null; sleep 2
openit
[ "$(first again)" = old ] && pass "opened again, the folder keeps its sort (by name)" || fail "reopened: $(first again2)"
v=$("$WINE" reg query 'HKCU\Software\Stained Glass\Explorer\FolderSorts' /v 'C:\sortgate' 2>/dev/null | tr -d '\r' | awk '/REG_DWORD/ { print $3 }')
[ "$v" = 0xa ] && pass "kept as FolderSorts\\C:\\sortgate = Name ascending (0xa)" || fail "FolderSorts value: '$v'"

echo
[ $RC = 0 ] && echo "explorer-sortmem-gate: PASS" || echo "explorer-sortmem-gate: FAIL"
exit $RC
