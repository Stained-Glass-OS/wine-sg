#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# runas of one's own prefix (wine-sg 7745). In a prefix of one's own (upstream Wine's single
# administrator, no Stained Glass consent broker) ShellExecuteEx "runas" starts the program elevated
# with the linked token. 7740 refused upstream Wine's linked token (identification level, like Windows
# hands it out) and started the program unelevated, and msiexec -- which relaunches itself with runas
# for an all-users package -- started itself without end (msifont, msiallusers and msitransform hung).
# Also: msiexec's runas relaunch never relaunches again. test/runasown-probe.c and
# test/msitransform-probe.c (the all-users package).
#   WINE=/opt/wine-sg/bin/wine test/runasown-gate.sh
# Mutants: SG_MUTANT_RUNAS_OWN_TOKEN_REFUSED (dlls/shell32/shlexec.c),
# SG_MUTANT_MSIEXEC_RELAUNCH_LOOP, SG_MUTANT_MSIEXEC_MARKER_KEPT (programs/msiexec/msiexec.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-runasown.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/runasown-probe.exe" "$HERE/runasown-probe.c" -lshell32 -lmsi &&
    TMPDIR=/var/tmp "$MINGW" -municode -O2 -o "$T/msitransform-probe.exe" "$HERE/msitransform-probe.c" -lmsi ||
    { echo "FAIL  probes did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/t"
cp "$T/runasown-probe.exe" "$T/msitransform-probe.exe" "$WINEPREFIX/drive_c/"

out=$(timeout -s KILL 120 "$WINE" 'C:\runasown-probe.exe' 2>/dev/null | tr -d '\r')
echo "$out"
printf '%s\n' "$out" | grep -qx 'parent elevated=0' && pass "the prefix owner's ordinary token is not elevated" \
    || fail "the parent was already elevated (nothing to test)"
printf '%s\n' "$out" | grep -qx 'child elevated=1' && pass "runas starts the program elevated, with the linked token" \
    || fail "runas did not elevate"

timeout -s KILL 120 "$WINE" 'C:\msitransform-probe.exe' 'C:\t' >/dev/null 2>&1
[ -s "$WINEPREFIX/drive_c/t/base.msi" ] || fail "the all-users package was not built"
SG_MSIEXEC_RUNAS=1 WINEDEBUG=+process timeout -s KILL 120 "$WINE" msiexec /i 'C:\t\base.msi' /qn >"$T/msiexec.log" 2>&1; rc=$?
n=$(grep -c 'CreateProcessInternalW app (null) cmdline .*msiexec' "$T/msiexec.log")
[ "$rc" != 137 ] && [ "$n" = 0 ] && pass "msiexec started by its own runas installs as it is, without another relaunch" \
    || fail "msiexec relaunched itself again (rc $rc, $n relaunches)"
q=$(timeout -s KILL 60 "$WINE" reg query 'HKLM\Software\SGTEST\TransformTest' /reg:64 2>/dev/null | tr -d '\r')
"$WINE" reg delete 'HKLM\Software\SGTEST' /f >/dev/null 2>&1
timeout -s KILL 300 "$WINE" msiexec /x 'C:\t\base.msi' /qn >/dev/null 2>&1
timeout -s KILL 300 "$WINE" msiexec /i 'C:\t\base.msi' /qn >/dev/null 2>&1; rc=$?
[ "$rc" = 0 ] && pass "an all-users msiexec install by the prefix owner elevates and finishes (rc 0)" \
    || fail "the all-users install (rc $rc)"
# the relaunch marker is msiexec's own: what it starts (a custom action, a nested msiexec for a
# second all-users package, which then elevates for itself) does not inherit it
out=$(timeout -s KILL 60 "$WINE" 'C:\runasown-probe.exe' makemsi 'C:\t\ca.msi' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qx 'makemsi=0' || fail "the custom action package was not built ($out)"
rm -f "$WINEPREFIX/drive_c/t/caenv.txt"
SG_MSIEXEC_RUNAS=1 timeout -s KILL 120 "$WINE" msiexec /i 'C:\t\ca.msi' /qn >/dev/null 2>&1; rc=$?
if [ -s "$WINEPREFIX/drive_c/t/caenv.txt" ]; then
    grep -qi '^SG_MSIEXEC_RUNAS=' "$WINEPREFIX/drive_c/t/caenv.txt" && fail "a custom action inherits the relaunch marker" \
        || pass "a custom action of the relaunched msiexec does not inherit its marker"
else
    fail "the custom action did not run (rc $rc)"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
