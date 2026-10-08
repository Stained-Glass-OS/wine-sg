#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Task definition parts that were E_NOTIMPL (patches/sg/1633), through a
# script: test/taskxml.js sets Data and the registration's
# SecurityDescriptor and reads them back from the registered task, reads
# and writes the XmlText of the registration info, settings and actions,
# and enumerates the actions and triggers (For Each / Enumerator).
#
#   WINE=/opt/wine-sg/bin/wine test/taskxml-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_TASK_DATA (taskschd/task.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskxml.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
cp "$HERE/taskxml.js" "$T/taskxml.js"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for arch in "" syswow64; do
    if [ -n "$arch" ]; then cs='C:\windows\syswow64\cscript.exe'; echo "== 32-bit"; else cs=cscript; echo "== 64-bit"; fi
    out=$(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$cs" //nologo 'Z:'"$(printf '%s' "$T/taskxml.js" | tr / '\\')" 2>&1 </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    v() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
    printf '%s\n' "$out" | grep -q '^error' && fail "a call failed: $(printf '%s\n' "$out" | grep '^error' | head -1)"
    [ "$(v actions)" = 2 ] && [ "$(v triggers)" = 2 ] && pass "the actions and triggers enumerate" || fail "enumerated: '$(v actions)' '$(v triggers)'"
    [ "$(v reginfoxml)" = true ] && pass "RegistrationInfo.XmlText" || fail "reginfoxml: '$(v reginfoxml)'"
    [ "$(v priority)" = 4 ] && pass "Settings.XmlText sets the settings" || fail "priority: '$(v priority)'"
    [ "$(v actionsxml)" = true ] && [ "$(v actionsput)" = 'C:\windows\regedit.exe' -o "$(v actionsput)" = '1 C:\windows\regedit.exe' ] &&
        pass "Actions.XmlText reads and replaces the actions" || fail "actions xml: '$(v actionsxml)' '$(v actionsput)'"
    [ "$(v data)" = sg-data ] && pass "TaskDefinition.Data is kept through registration" || fail "data: '$(v data)'"
    [ "$(v sd)" = 'D:(A;;FA;;;BA)' ] && pass "RegistrationInfo.SecurityDescriptor too" || fail "sd: '$(v sd)'"
    printf '%s\n' "$out" | grep -qx done && pass "the script finished" || fail "the script did not finish"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
