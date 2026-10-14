#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wscript.exe / cscript.exe (patches/sg/5524): WScript.GetObject (a moniker
# name, a ProgID with an empty path, a ProgID with a file to load) and the
# report of an unhandled script error with exit code 1.
#
#   WINE=/opt/wine-sg/bin/wine test/wscript-gate.sh
# Mutant (programs/wscript/host.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_WSH_GETOBJECT_NOTIMPL"):
# GetObject of a moniker name fails.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wscript.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

run() { DISPLAY= timeout -s KILL 60 "$WINE" cscript //nologo "Z:$1" > "$T/out" 2> "$T/err"; RET=$?; tr -d '\r' < "$T/out" > "$T/o"; tr -d '\r' < "$T/err" > "$T/e"; }

cat > "$T/moniker.vbs" <<'EOS'
Set d = WScript.GetObject("new:{EE09B103-97E0-11CF-978F-00A02463E06F}")
d.Add "k", "moniker"
WScript.Echo d("k")
EOS
run "$T/moniker.vbs"
[ $RET = 0 ] && [ "$(cat "$T/o")" = moniker ] && pass "GetObject of a moniker name returns the object" || fail "moniker ($RET): $(cat "$T/o" "$T/e")"

cat > "$T/progid.vbs" <<'EOS'
Set d = WScript.GetObject("", "Scripting.Dictionary")
d.Add "k", "progid"
WScript.Echo d("k")
EOS
run "$T/progid.vbs"
[ $RET = 0 ] && [ "$(cat "$T/o")" = progid ] && pass "GetObject with an empty path creates the ProgID's object" || fail "progid ($RET): $(cat "$T/o" "$T/e")"

cat > "$T/none.vbs" <<'EOS'
On Error Resume Next
Set d = WScript.GetObject("", "")
If Err.Number <> 0 Then WScript.Echo "failed"
EOS
run "$T/none.vbs"
[ "$(cat "$T/o")" = failed ] && pass "GetObject with neither a path nor a ProgID is an error" || fail "none: $(cat "$T/o" "$T/e")"

cat > "$T/err.vbs" <<'EOS'
WScript.Echo "before"
x = 1 / 0
WScript.Echo "after"
EOS
run "$T/err.vbs"
if [ $RET = 1 ] && grep -q "^before\$" "$T/o" && ! grep -q "^after\$" "$T/o" && grep -q "err.vbs(2, [0-9]*) .*: .*[Dd]ivision by zero" "$T/o"
then pass "an unhandled error is reported with file, line and column and exits 1; the script stops there"; else fail "error report ($RET): $(cat "$T/o" "$T/e")"; fi

cat > "$T/ok.vbs" <<'EOS'
WScript.Echo "fine"
EOS
run "$T/ok.vbs"
[ $RET = 0 ] && pass "a script without errors exits 0" || fail "ok script: $RET"

[ $RC = 0 ] && echo "wscript-gate: all passed" || echo "wscript-gate: FAILURES"
exit $RC
