#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Task Scheduler's objects through IDispatch (patches/sg/1631): every
# GetTypeInfoCount, GetTypeInfo, GetIDsOfNames and Invoke was E_NOTIMPL, so
# no script (VBScript, JScript, PowerShell's COM, installers' custom
# actions) could make or read a scheduled task. test/taskdisp.js registers
# a task through "Schedule.Service" under cscript, reads it back and
# deletes it.
#
#   WINE=/opt/wine-sg/bin/wine test/taskdisp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_DISPATCH (taskschd/taskschd.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskdisp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
cp "$HERE/taskdisp.js" "$T/taskdisp.js"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for arch in "" syswow64; do
    if [ -n "$arch" ]; then cs='C:\windows\syswow64\cscript.exe'; echo "== 32-bit"; else cs=cscript; echo "== 64-bit"; fi
    out=$(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$cs" //nologo 'Z:'"$(printf '%s' "$T/taskdisp.js" | tr / '\\')" 2>&1 </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    v() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
    [ "$(v folder)" = '\' ] && pass "Connect and GetFolder through IDispatch" || fail "folder: '$(v folder)'"
    [ "$(v registered)" = "SG Dispatch Probe" ] && pass "a task defined and registered from a script" || fail "registered: '$(v registered)'"
    [ "$(v description)" = "SG dispatch probe" ] && pass "... its description reads back" || fail "description: '$(v description)'"
    [ "$(v trigger)" = "2030-01-01T10:00:00" ] && pass "... and its trigger" || fail "trigger: '$(v trigger)'"
    printf '%s\n' "$out" | grep -qx done && pass "... and it is deleted" || fail "the script did not finish"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
