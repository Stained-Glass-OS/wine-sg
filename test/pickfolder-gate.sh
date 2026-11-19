#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder picker's button with no name typed (patches/sg/7906). IFileOpenDialog with
# FOS_PICKFOLDERS: Windows' "Select Folder" with the name box empty picks the folder being shown.
# Wine's did nothing (on_default_action returned at once with no name): DOSBox-X's first-run
# "Choose a working folder" kept its dialog open whatever was clicked. test/pickfolder-probe.c
# presses the dialog's OK button with the name box empty, then with a subfolder's name typed
# (unchanged), on an Xvfb of its own.
#   WINE=/opt/wine-sg/bin/wine test/pickfolder-gate.sh
# Mutant: -DSG_MUTANT_PICKFOLDER_EMPTY_NAME (dlls/comdlg32/itemdlg.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc Xvfb; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pickfolder.XXXXXX); XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/pickfolder-probe.c" -lole32 -lshell32 -luuid -luser32 ||
    { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1$"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "^$1=[^ ]*" | head -1 || echo none))"; fi
}
check emptyname "OK with no name typed picks the folder shown (C:\\pick)"
check typedname "OK with a subfolder's name typed picks that subfolder"
check done      "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
