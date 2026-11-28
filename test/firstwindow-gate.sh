#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The first program to make windows has its desktop's shell (patches/sg/9022). 9061 let a process
# start a desktop's explorer only once; a second thread that asked while the first was still starting
# it made the desktop window itself with no shell and no display driver behind it, and the program's
# windows all failed with "The explorer process failed to start". It happened to the first program
# after a new account's first sign-in or after an update restarted the server, whenever explorer was
# slow to come up and two of the program's threads made windows at once (the delete question's dialog
# and the thread looking for it; a splash screen and a main window). On Windows the shell's desktop is
# there before programs. Four threads make their first window at the same moment, each time on a
# server just started (wineserver -k; -w), three times; test/firstwindow-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/firstwindow-gate.sh
# Mutant: win32u built with -DSG_MUTANT_SHELL_NO_WAIT (the second thread does not wait).
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
T=$(mktemp -d /var/tmp/sg-firstwindow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+winediag WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/fwprobe.exe" "$HERE/firstwindow-probe.c" || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
cp "$T/fwprobe.exe" "$WINEPREFIX/drive_c/fwprobe.exe"
for i in 1 2 3; do
    out=$(timeout -s KILL 120 "$WINE" 'C:\fwprobe.exe' 2>"$T/err$i" | tr -d '\r')
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
    case "$out" in
    *"made 4 of 4"*) pass "run $i on a new server: all four threads' first windows made" ;;
    *) fail "run $i on a new server: $(echo "$out" | tr '\n' ' ')" ;;
    esac
    if grep -q 'explorer process failed to start' "$T/err$i"; then
        fail "run $i: 'The explorer process failed to start'"
    else
        pass "run $i: no 'The explorer process failed to start'"
    fi
done
exit $RC
