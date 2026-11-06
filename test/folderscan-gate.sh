#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder is not read again and again to be shown (patches/sg/0782): File
# Explorer showing a folder of 520 items asked for its desktop.ini hundreds of
# times (each folder's icon, each binding), and a missing desktop.ini is a
# search of the whole folder -- on a network share, that many listings of it
# over the network: slow to draw, drawn twice (David's list, item 29). Counted
# here with strace: the folder opened as a directory, its desktop.ini looked
# up, and directory reads, while File Explorer shows it.
#
#   WINE=/opt/wine-sg/bin/wine test/folderscan-gate.sh   (mutant SG_MUTANT_NO_INI_CACHE)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in strace Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-folderscan.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
B="$WINEPREFIX/drive_c/big"; mkdir -p "$B"
i=1; while [ $i -le 500 ]; do echo "file $i" > "$B/doc$i.txt"; i=$((i + 1)); done
i=1; while [ $i -le 20 ]; do mkdir "$B/dir$i"; i=$((i + 1)); done
timeout -s KILL 60 strace -f -qq -e trace=openat,newfstatat,getdents64 -o "$T/st.txt" "$WINE" explorer 'C:\big' >/dev/null 2>&1 &
sleep 25
"$WINESERVER" -k; sleep 2
opens=$(grep -c 'c:/big", O_RDONLY|O_DIRECTORY' "$T/st.txt")
inis=$(grep -c 'c:/big/desktop.ini' "$T/st.txt")
reads=$(grep -c getdents64 "$T/st.txt")
grep -q 'doc500.txt' "$T/st.txt" && pass "File Explorer read the folder (doc500.txt seen)" || fail "the folder was not read"
[ "$opens" -le 5 ] && pass "the folder opened $opens times to be shown (it was 124)" || fail "the folder opened $opens times"
[ "$inis" -le 8 ] && pass "its desktop.ini looked up $inis times (it was 248)" || fail "desktop.ini looked up $inis times"
[ "$reads" -le 400 ] && pass "$reads directory reads (it was 1423)" || fail "$reads directory reads"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
