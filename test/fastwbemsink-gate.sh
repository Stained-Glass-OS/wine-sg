#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemdisp: SWbemSink and the asynchronous operations -- the events (OnObjectReady,
# OnCompleted) reach the sinks connected through the connection point.
#
#   WINE=/opt/wine-sg/bin/wine test/fastwbemsink-gate.sh   (mutant SG_MUTANT_WBEM_NO_COMPLETED)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastwbemsink.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastwbemsink-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q '^DONE' "$T/o" || fail "the probe did not finish: $(tail -2 "$T/o" | tr '\n' ' ')"
[ "$(v SINK)" = 00000000 ] && [ "$(v CP)" = 00000000 ] && [ "$(v CP_OTHER)" = 1 ] && pass "SWbemSink is a class with a connection point for its events (and no other)" || fail "sink: $(grep '^SINK\|^CP' "$T/o" | tr '\n' ' ')"
[ "$(v SRCIID)" = 1 ] && [ "$(v CLASSINFO)" = 00000000 ] && [ "$(v CLASSKIND)" = "5 impl 2" ] && pass "its class info names the events as the default source" || fail "class info: $(grep '^SRC\|^CLASS' "$T/o" | tr '\n' ' ')"
[ "$(v GETASYNC)" = "00000000 ready 1 class Win32_Process completed 1 hres 0 ctx 1" ] && pass "GetAsync: OnObjectReady once with the object, OnCompleted(0), the context back" || fail "GetAsync: $(v GETASYNC)"
[ "$(v SYNC_COUNT)" = 1 ] && [ "$(v QUERYASYNC)" = "00000000 every 1 completed 1" ] && pass "ExecQueryAsync: an event for each object, one completion" || fail "ExecQueryAsync: $(v QUERYASYNC)"
[ "$(v INSTANCESASYNC)" = "00000000 every 1 completed 1" ] && pass "InstancesOfAsync" || fail "InstancesOfAsync: $(v INSTANCESASYNC)"
[ "$(v BADQUERY)" = "1 completed 0" ] && pass "a bad query is refused at the call" || fail "bad query: $(v BADQUERY)"
case "$(v NOTASINK)" in 80020009*) pass "something that is not a sink is refused (the call raises WBEM_E_INVALID_PARAMETER)";; *) fail "not a sink: $(v NOTASINK)";; esac
[ "$(v UNADVISE)" = 00000000 ] && [ "$(v UNADVISE2)" = 1 ] && [ "$(v UNCONNECTED)" = "00000000 events 0" ] && pass "unadvised, a sink hears nothing (and the call is fine)" || fail "unadvise: $(grep '^UNADV\|^UNCONN' "$T/o" | tr '\n' ' ')"
[ "$(v CANCEL)" = 00000000 ] && pass "Cancel with nothing pending" || fail "Cancel: $(v CANCEL)"
# a script gets the events through a prefix (WScript.CreateObject, ConnectObject)
cp "$HERE/fastwbemsink.vbs" "$WINEPREFIX/drive_c/t.vbs"
timeout -s KILL 180 "$WINE" cscript //nologo 'C:\t.vbs' 2>/dev/null </dev/null | tr -d '\r' > "$T/vbs.out"
EXPECT=$(grep -c '^ *Check "' "$HERE/fastwbemsink.vbs")
GOT=$(grep -c '^PASS' "$T/vbs.out")
grep -v '^PASS' "$T/vbs.out"
[ "$EXPECT" = "$GOT" ] && grep -q '^RESULT: PASS' "$T/vbs.out" && pass "a script hears the events by prefix: $GOT checks" || fail "script: $GOT of $EXPECT checks"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
