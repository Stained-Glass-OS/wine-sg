#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wineserver's ptrace wait (6259): a thread that cannot stop within the server's 3 s watchdog
# (here in a vfork) kept PTRACE_ATTACH's SIGSTOP pending after the server let go of it, and the
# process stopped for good (state T). test/ptracestop-probe.c; the target must not be stopped
# afterwards and must finish. Mutant: SG_MUTANT_PTRACE_STOP_PENDING (server/ptrace.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ "$(uname -m)" = x86_64 ] || { echo "SKIP: x86_64 only"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ptracestop.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { [ -n "${UPID:-}" ] && kill -CONT "$UPID" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/ptracestop-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
D='C:\ptracestop'
mkdir -p "$WINEPREFIX/drive_c/ptracestop"
"$WINE" "$T/probe.exe" target "$D" > "$T/target.out" 2>/dev/null </dev/null &
r=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" reader "$D" 2>/dev/null </dev/null | tr -d '\r')
UPID=$(tr -d '\r' < "$T/target.out" | sed -n 's/^unixpid //p')
sleep 2
STATE=$( [ -n "$UPID" ] && awk '{print $3}' /proc/$UPID/stat 2>/dev/null)
sleep 6
tr -d '\r' < "$T/target.out" | sed 's/^/      target: /'
printf '      reader: %s\n      state after: %s\n' "$r" "${STATE:-gone}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -n "$UPID" ] && pass "the target is pid $UPID" || fail "no target pid"
printf '%s\n' "$r" | grep -q '^read ' && pass "the reader read: $r" || fail "no read: $r"
[ "${STATE:-}" != T ] && [ "${STATE:-}" != t ] && pass "the target is not left stopped (state ${STATE:-gone})" || fail "the target is stopped (state $STATE)"
tr -d '\r' < "$T/target.out" | grep -qx returned && pass "the target went on after its vfork" || fail "the target never returned from its vfork"
printf '%s\n' "$r" | grep -q '^suspend 0 context 1 rip 1$' && pass "its thread can be suspended and looked at afterwards" || fail "its thread afterwards: $(printf '%s\n' "$r" | grep '^suspend')"
printf '%s\n' "$r" | grep -q '^read again 1' && pass "and its memory read" || fail "read again: $(printf '%s\n' "$r" | grep '^read again')"
tr -d '\r' < "$T/target.out" | grep -q '^alive [1-9]' && pass "the target ran on to its end" || fail "the target did not run on"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
