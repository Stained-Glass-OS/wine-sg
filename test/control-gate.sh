#!/bin/sh
# control.exe follows App Paths (patches/sg/0072).
#
# A program that runs system32\control.exe itself -- not through
# ShellExecute -- must get the Control Panel App Paths names (sg-shell's),
# with its arguments, and that one must see the hand-off marker so it can
# run control.exe back without a loop.
#
#   WINE=/opt/wine-sg/bin/wine test/control-gate.sh
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
T=$(mktemp -d /var/tmp/sg-control.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/control-probe.exe" "$HERE/control-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/control-probe.exe" "$WINEPREFIX/drive_c/"
# the stand-in Control Panel: App Paths can hold only a path, so a wrapper
# batch is not possible -- the probe itself, told apart by an argument
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\control.exe' /ve /d 'C:\control-probe.exe' /f >/dev/null 2>&1
"$WINESERVER" -w
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 xvfb-run -a "$WINE" control-probe.exe run appwiz.cpl 2>/dev/null | tr -d '\r')
"$WINESERVER" -w
log=$(tr -d '\r' < "$WINEPREFIX/drive_c/standin.log" 2>/dev/null)
printf '%s\n' "$log" | sed 's/^/      /'
case "$log" in *"cmdline="*"appwiz.cpl"*) pass "control.exe run directly hands off to the App Paths Control Panel, with its arguments" ;;
    *) fail "no hand-off (the stand-in did not run): $out" ;; esac
case "$log" in *"handoff=1"*) pass "which sees the hand-off marker (no loop back)" ;; *) fail "no SG_CONTROL_HANDOFF in the stand-in" ;; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
