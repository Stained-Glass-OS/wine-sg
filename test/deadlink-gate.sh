#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A shortcut whose program is gone (patches/sg/5966): Zoom Workplace's uninstaller left its
# Public desktop shortcut, and opening it said "Path not found."; with 5965 an Open with for the
# .lnk then came up too. Windows asks "Problem with Shortcut" -- delete it? -- and nothing more.
# A shortcut's own handler that ran and failed ends the open (its error kept); Open with is never
# offered for a .lnk; a working shortcut still starts its program. test/deadlink-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/deadlink-gate.sh
# Mutants: SG_MUTANT_DEAD_LINK_ERROR (shelllink.c: no question), SG_MUTANT_LINK_FALLTHROUGH
# (shlexec.c: a failed shortcut is taken for the file itself).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-deadlink.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/deadlink-probe.c" -lshell32 -lole32 -luuid ||
    { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
check dead_asks          "opening a shortcut whose program is gone asks \"Problem with Shortcut\" (mutant DEAD_LINK_ERROR)"
check dead_nothing_else  "and nothing else comes up: no \"Path not found.\", no Open with (mutant LINK_FALLTHROUGH)"
check dead_yes_deletes   "Yes deletes the shortcut"
check dead_cancelled     "and ShellExecuteEx fails with ERROR_CANCELLED"
check dead_no_keeps      "No keeps it, nothing more"
check noui_quiet         "with SEE_MASK_FLAG_NO_UI: ERROR_FILE_NOT_FOUND, nothing shown"
check live_runs          "a shortcut whose program is there starts it (exit code 7 handed back)"
check bad_one_error      "a target that is there but cannot start: one error box, no Open with, its error kept"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
