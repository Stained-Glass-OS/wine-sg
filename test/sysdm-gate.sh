#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# sysdm.cpl, System Properties (patches/sg/0757): a front door to the Control
# Panel's own. "rundll32 sysdm.cpl,EditEnvironmentVariables" -- what
# installers and guides give to change PATH -- opens its Environment
# Variables dialog, and sysdm.cpl opened as a Control Panel item (Run
# "sysdm.cpl") its System page (David 2026-10-01: Claude Code asked for PATH
# to be changed). Without sysdm.cpl both said the file was not found.
#
# The Control Panel is a stand-in (control-probe, through App Paths, as
# control-gate.sh) recording what it was asked for.
#
#   WINE=/opt/wine-sg/bin/wine test/sysdm-gate.sh
# Mutant: SG_MUTANT_SYSDM_LOOPS (sysdm.c, 1512).
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
T=$(mktemp -d /var/tmp/sg-sysdm.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/control-probe.exe" "$HERE/control-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/control-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\control.exe' /ve /d 'C:\control-probe.exe' /f >/dev/null 2>&1
"$WINESERVER" -w
[ -f "$WINEPREFIX/drive_c/windows/system32/sysdm.cpl" ] && pass "system32 has sysdm.cpl" || fail "no system32\\sysdm.cpl"
L="$WINEPREFIX/drive_c/standin.log"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 xvfb-run -a "$WINE" rundll32 sysdm.cpl,EditEnvironmentVariables >/dev/null 2>&1)
"$WINESERVER" -w
grep -q 'cmdline=.*EditEnvironmentVariables' "$L" 2>/dev/null \
    && pass "rundll32 sysdm.cpl,EditEnvironmentVariables opens the Control Panel's Environment Variables" \
    || fail "EditEnvironmentVariables: $(tr -d '\r' < "$L" 2>/dev/null)"
rm -f "$L"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 xvfb-run -a "$WINE" rundll32 shell32.dll,Control_RunDLL sysdm.cpl >/dev/null 2>&1)
"$WINESERVER" -w
# System Properties is the Control Panel's own page (control sysdm.cpl), not
# "/name Microsoft.System" -- Settings > About in Windows 10 (1512)
grep -q 'cmdline=.*sysdm\.cpl' "$L" 2>/dev/null && ! grep -q 'Microsoft.System' "$L" 2>/dev/null \
    && pass "sysdm.cpl opened as a Control Panel item (Run \"sysdm.cpl\") asks for System Properties (control sysdm.cpl)" \
    || fail "sysdm.cpl: $(tr -d '\r' < "$L" 2>/dev/null)"
# with Wine's own control.exe (no Stained Glass Control Panel), that would host
# sysdm.cpl again, which would start it again: once is enough (1512)
"$WINE" reg delete 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\control.exe' /f >/dev/null 2>&1
"$WINESERVER" -w
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 xvfb-run -a "$WINE" rundll32 shell32.dll,Control_RunDLL sysdm.cpl >/dev/null 2>&1); rc=$?
[ "$rc" != 137 ] && pass "with Wine's own control.exe it ends (rc $rc) -- no endless control.exe/sysdm.cpl loop" \
    || fail "sysdm.cpl and Wine's control.exe started each other until killed (60 s)"
"$WINESERVER" -k 2>/dev/null
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
