#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Save As > Desktop (the places bar) shows what is on the desktop -- its
# folder's files -- not the namespace root (patches/sg/0480). David: in
# Notepad's Save As, Desktop showed "Documents" and "This PC" links among the
# files, though saving there went to the right folder.
#
#   WINE=/opt/wine-sg/bin/wine test/savedesk-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-savedesk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/savedesk-probe.exe" "$HERE/savedesk-probe.c" -lcomdlg32 -lshell32 -lcomctl32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 xvfb-run -a "$WINE" "$T/savedesk-probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
grep -q '^item gate-desktop' "$T/out" && pass "Desktop shows the desktop's files" || fail "the desktop's file is not listed"
grep -qE '^item (This PC|My Computer|Documents|My Documents|Computer)$' "$T/out" \
    && fail "Desktop lists namespace links among the files" || pass "and no Documents / This PC links"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
