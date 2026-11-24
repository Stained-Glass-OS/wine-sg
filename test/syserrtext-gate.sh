#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The system message table has text for every Windows Sockets error and the common COM HRESULTs
# (patches/sg/9750). Wine's had 13 of the Windows Sockets codes and 3 HRESULTs, so
# FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM) failed with ERROR_MR_MID_NOT_FOUND (317) for the rest:
# Lapce showed "tcp connect error: OS Error 10051 (FormatMessageW() returned error 317)" (Rust's
# std formats every OS error that way), Chromium-based programs logged "Error (0x13D) while
# retrieving error. (0x80004002)". The probe (test/syserrtext-probe.c) asks for each code:
#   every Windows Sockets code winerror.h defines (WSAEINTR .. WSA_QOS_RESERVED_PETYPE) has text
#   the COM ones (E_NOINTERFACE, E_POINTER, E_ABORT, E_UNEXPECTED, E_PENDING .. RO_E_CLOSED, CLASS_E_*,
#   REGDB_E_CLASSNOTREG, CO_E_NOTINITIALIZED, RPC_E_*, DISP_E_*) have text
#   HRESULT_FROM_WIN32(WSAENETUNREACH) folds to the same text
#   WINE=/opt/wine-sg/bin/wine test/syserrtext-gate.sh
# No mutant: the table is a message file (kernelbase/winerror.mc), not C; the gate FAILs on 10.0-1140.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-syserrtext.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/syserrtext-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
miss() { printf '%s\n' "$out" | awk -v lo="$1" -v hi="$2" '$2 == "NONE" && $1 >= lo && $1 <= hi { printf "%s ", $1 }'; }
n=$(printf '%s\n' "$out" | grep -c ' ok$')
printf '      %s codes have text\n' "$n"
m=$(miss 00002700 00002b07); [ -z "$m" ] && [ "$n" -gt 0 ] && pass "every Windows Sockets error has text" || fail "Windows Sockets errors without text: $m"
m=$(miss 80000000 8000ffff); [ -z "$m" ] && [ "$n" -gt 0 ] && pass "E_NOINTERFACE, E_POINTER, E_ABORT, E_UNEXPECTED, E_PENDING .. RO_E_CLOSED have text" || fail "without text: $m"
m=$(miss 80010000 8004ffff); [ -z "$m" ] && [ "$n" -gt 0 ] && pass "RPC_E_*, DISP_E_*, CLASS_E_*, REGDB_E_CLASSNOTREG, CO_E_* have text" || fail "without text: $m"
m=$(miss 80070000 8007ffff); [ -z "$m" ] && [ "$n" -gt 0 ] && pass "HRESULT_FROM_WIN32(WSAENETUNREACH) folds to its text" || fail "without text: $m"
s=$(printf '%s\n' "$out" | sed -n 's/^SAMPLE //p')
printf '      10051: %s\n' "$s"
[ -n "$s" ] && [ "$s" != none ] && pass "WSAENETUNREACH reads as a sentence" || fail "WSAENETUNREACH: '$s'"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
