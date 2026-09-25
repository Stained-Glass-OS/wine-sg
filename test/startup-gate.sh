#!/bin/sh
# A startup item turned off in Task Manager does not start (patches/sg/0125).
#
# Windows keeps Task Manager's Startup tab choices in
# Explorer\StartupApproved\{Run,Run32,StartupFolder}: a binary value per entry,
# first byte odd when disabled. wineboot, starting the Run keys and the Startup
# folder, must skip what is disabled there, start what is enabled or not listed,
# and never apply it to RunOnce.
#
#   WINE=/opt/wine-sg/bin/wine test/startup-gate.sh
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
T=$(mktemp -d /var/tmp/sg-startup.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe.exe" "$HERE/handoff-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/standin.exe"
R='Software\Microsoft\Windows\CurrentVersion'
A="$R\\Explorer\\StartupApproved"
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg "HKCU\\$R\\Run" /v UserOn /d 'C:\standin.exe user-on'
reg "HKCU\\$R\\Run" /v UserOff /d 'C:\standin.exe user-off'
reg "HKCU\\$R\\Run" /v UserApproved /d 'C:\standin.exe user-approved'
reg "HKLM\\$R\\Run" /v MachineOff /d 'C:\standin.exe machine-off'
reg "HKLM\\$R\\Run" /v MachineOn /d 'C:\standin.exe machine-on'
reg "HKLM\\$R\\Run" /v Wow32Off /d 'C:\standin.exe wow32-off' /reg:32
reg "HKLM\\$R\\RunOnce" /v OnceListed /d 'C:\standin.exe once'
reg "HKCU\\$A\\Run" /v UserOff /t REG_BINARY /d 030000000000000000000000
reg "HKCU\\$A\\Run" /v UserApproved /t REG_BINARY /d 020000000000000000000000
reg "HKLM\\$A\\Run" /v OnceListed /t REG_BINARY /d 030000000000000000000000
reg "HKLM\\$A\\Run" /v MachineOff /t REG_BINARY /d 070000000000000000000000
reg "HKLM\\$A\\Run32" /v Wow32Off /t REG_BINARY /d 030000000000000000000000
reg "HKCU\\$A\\StartupFolder" /v folder-off.exe /t REG_BINARY /d 030000000000000000000000
S=$(find "$C/users" -path '*Start Menu/Programs/StartUp' -type d | grep -v Public | head -1)
[ -n "$S" ] || { mkdir -p "$C/users/$(id -un)/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/StartUp"; S="$C/users/$(id -un)/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/StartUp"; }
cp "$T/probe.exe" "$S/folder-on.exe"; cp "$T/probe.exe" "$S/folder-off.exe"
"$WINESERVER" -w
# a session's start: wineboot runs the Run keys and the Startup folder
(cd "$C" && timeout -s KILL 120 xvfb-run -a "$WINE" wineboot >/dev/null 2>&1)
i=0; while [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
"$WINESERVER" -w
log=$(tr -d '\r' < "$C/standin.log" 2>/dev/null)
printf '%s\n' "$log" | sed 's/^/      /'
has() { printf '%s\n' "$log" | grep -q -- "$1"; }
has 'user-on'       && pass "an HKCU Run entry with no StartupApproved value starts" || fail "user-on did not start"
has 'user-approved' && pass "one approved (02) starts" || fail "user-approved did not start"
has 'machine-on'    && pass "an HKLM Run entry starts" || fail "machine-on did not start"
has 'user-off'      && fail "a disabled (03) HKCU Run entry started" || pass "a disabled (03) HKCU Run entry does not start"
has 'machine-off'   && fail "a disabled (07) HKLM Run entry started" || pass "a disabled (07) HKLM Run entry does not start"
has 'wow32-off'     && fail "a disabled 32-bit Run entry (Run32) started" || pass "a disabled 32-bit Run entry (StartupApproved\\Run32) does not start"
has ' once'         && pass "RunOnce ignores StartupApproved, as on Windows" || fail "RunOnce entry did not run"
case "$log" in *folder-on.exe*) pass "a Startup folder item starts" ;; *) fail "folder-on.exe did not start" ;; esac
case "$log" in *folder-off.exe*) fail "a disabled Startup folder item started" ;; *) pass "a disabled Startup folder item (StartupApproved\\StartupFolder) does not start" ;; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
