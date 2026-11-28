#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The delete question's details (patches/sg/9066). Windows asks "Are you
# sure you want to move this file to the Recycle Bin?" and shows under it the
# item's own icon (no question icon), its name, "Type:", "Size:" and "Date
# modified:"; for a folder its name and "Date created:"; for several items
# only the question. We showed the item's full path there.
#
#   WINE=/opt/wine-sg/bin/wine test/deleteask-gate.sh
# Mutant: shell32 built with -DSG_MUTANT_CONFIRM_NO_DETAILS (the path again).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-deleteask.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/daprobe.exe" "$HERE/deleteask-probe.c" -lshell32 -luser32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
C="$WINEPREFIX/drive_c"
cp "$T/daprobe.exe" "$C/daprobe.exe"
probe() { timeout -s KILL 60 "$WINE" 'C:\daprobe.exe' "$@" 2>/dev/null | tr -d '\r'; }
line() { echo "$1" | sed -n "s/^$2: //p" | head -1; }
field() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }

head -c 1000 /dev/zero > "$C/report.txt"
touch -d '2026-03-04 15:05' "$C/report.txt"
mkdir -p "$C/Photos"; head -c 10 /dev/zero > "$C/Photos/a.bin"
head -c 10 /dev/zero > "$C/two.txt"
# the first program to open a window after wineboot finds no desktop yet ("The
# explorer process failed to start", released builds too): one that asks nothing first
head -c 1 /dev/zero > "$C/warm.txt"; probe delete 'C:\warm.txt' >/dev/null

# ---- one file to the bin ------------------------------------------------------
out=$(probe trash 'C:\report.txt'); echo "$out" | sed 's/^/      /'
[ "$(line "$out" caption)" = "Delete File" ] \
    && [ "$(line "$out" question)" = "Are you sure you want to move this file to the Recycle Bin?" ] \
    && pass "one file: 'Delete File', 'Are you sure you want to move this file to the Recycle Bin?' alone" \
    || fail "one file's caption/question"
d=$(line "$out" details)
case "$d" in
report*" / Type: Text Document / Size: 1000 bytes / Date modified: "*3*4*2026*3:05*)
    pass "under it: the name, 'Type: Text Document', 'Size: 1000 bytes', 'Date modified: 3/4/2026 3:05 PM'" ;;
*)  fail "the file's details: '$d'" ;;
esac
echo "$out" | grep -v '^details:' | grep -qF 'report.txt' && fail "the path is still shown" || pass "no path under the question"
[ "$(field "$out" qicon)" = 0 ] && pass "no question icon: the item's own icon instead" || fail "question icon shown ($out)"
n=$(field "$out" drawn)
[ "${n:-0}" -gt 600 ] && pass "an icon drawn left, under the question ($n pixels)" || fail "no icon under the question ($n pixels)"
[ -f "$C/report.txt" ] && pass "No keeps the file" || fail "No deleted the file"

# ---- one file for good ---------------------------------------------------------
out=$(probe delete 'C:\report.txt')
[ "$(line "$out" caption)" = "Delete File" ] \
    && [ "$(line "$out" question)" = "Are you sure you want to permanently delete this file?" ] \
    && case "$(line "$out" details)" in report*" / Type: Text Document / Size: 1000 bytes / Date modified: "*) true ;; *) false ;; esac \
    && pass "for good (Shift+Delete): 'permanently delete this file?' with the same details" \
    || fail "permanent delete: $(echo "$out" | tr '\n' '|')"

# ---- a folder ----------------------------------------------------------------
out=$(probe trash 'C:\Photos'); echo "$out" | sed 's/^/      /'
d=$(line "$out" details)
[ "$(line "$out" caption)" = "Delete Folder" ] \
    && [ "$(line "$out" question)" = "Are you sure you want to move this folder to the Recycle Bin?" ] \
    && case "$d" in "Photos / Date created: "*) true ;; *) false ;; esac \
    && ! echo "$d" | grep -q 'Size:' \
    && pass "a folder: 'Delete Folder', its name and 'Date created:' (no type or size)" \
    || fail "the folder's question: $(echo "$out" | tr '\n' '|')"

# ---- several items: the question alone, with its icon ---------------------------
out=$(probe trash 'C:\two.txt' 'C:\Photos')
[ "$(line "$out" caption)" = "Delete Multiple Items" ] \
    && [ "$(line "$out" question)" = "Are you sure you want to move these 2 items to the Recycle Bin?" ] \
    && [ -z "$(line "$out" details)" ] && [ "$(field "$out" qicon)" = 1 ] \
    && pass "several items: the question alone, with the bin's icon" \
    || fail "several items: $(echo "$out" | tr '\n' '|')"

"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
sg_kill_prefix "$WINEPREFIX"
[ $RC = 0 ] && echo "deleteask-gate: all PASS" || echo "deleteask-gate: FAIL"
exit $RC
