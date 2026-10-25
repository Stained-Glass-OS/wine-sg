#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The WMI refresher (6103). CLSID_WbemRefresher was not registered: CPU-Z and other hardware and
# performance monitors create one to read objects and instances again and again. Now: AddEnum on a class
# (IWbemHiPerfEnum: GetObjects gives IWbemObjectAccess objects, read with property handles; Refresh reads
# the instances again -- a process started after AddEnum appears, one that ended goes), AddObjectByPath
# (an object refreshed in place), Remove.
#   WINE=/opt/wine-sg/bin/wine test/wbemrefresher-gate.sh
# Mutant: SG_MUTANT_NO_REFRESHER (wbemprox main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v $MINGW >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wbemrefresher.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wbemrefresher-probe.c" -lole32 -loleaut32 -luuid -lwbemuuid 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 90 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v done)" = 1 ] || { fail "the probe did not finish"; echo "RESULT: FAIL"; exit 1; }
[ "$(v refresher)" = 0 ] && [ "$(v configure)" = 0 ] && pass "CLSID_WbemRefresher: IWbemRefresher and IWbemConfigureRefresher" \
    || fail "refresher: create $(v refresher), configure $(v configure)"
[ "$(v addenum)" = 0 ] && [ "$(v count)" = 1 ] && pass "AddEnum(Win32_Process) gives the processes, read through IWbemObjectAccess handles" \
    || fail "AddEnum: $(v addenum), count $(v count)"
[ "$(v before_refresh)" = 0 ] && [ "$(v after_refresh)" = 1 ] && [ "$(v refresh)" = 0 ] \
    && pass "a process started after AddEnum appears at the next Refresh, not before" \
    || fail "refresh: before $(v before_refresh), after $(v after_refresh) (hr $(v refresh))"
[ "$(v after_exit)" = 0 ] && pass "and goes when it has ended" || fail "after the child ended: $(v after_exit)"
[ "$(v addobject)" = 0 ] && [ "$(v access)" = 0 ] && [ "$(v object_pid)" = 1 ] && [ "$(v qword)" = 1 ] \
    && pass "AddObjectByPath: the object, its ProcessId (ReadDWORD) and WorkingSetSize (ReadQWORD)" \
    || fail "AddObjectByPath: $(v addobject) access $(v access) pid $(v object_pid) qword $(v qword)"
[ "$(v object_refresh)" = 0 ] && [ "$(v object_after)" = 1 ] && pass "refreshed in place, still the same process" \
    || fail "object refresh: $(v object_refresh), after $(v object_after)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
