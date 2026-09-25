#!/bin/sh
# Windows.System.DispatcherQueue (patches/sg/0174).
#
# CreateDispatcherQueueController returned E_NOTIMPL, so programs built on
# DispatcherQueue (WinUI / Windows App SDK programs, Paint.NET's installer)
# stopped at their first line.
#
#   WINE=/opt/wine-sg/bin/wine test/dispatcherq-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dispatcherq.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/dispatcherq-probe.exe" "$HERE/dispatcherq-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/dispatcherq-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -q -- "$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2"; fi; }
check '^create_current=0$' "CreateDispatcherQueueController for the calling thread"
check '^enqueued=111 order=HNL$' "work runs by priority: high, normal, low"
check '^statics=1 current_is_queue=1$' "DispatcherQueue.GetForCurrentThread finds it"
check 'access=1 other_access=0' "HasThreadAccess: this thread, not another"
check 'other_enqueued=1 ran_here=1 other_has_none=1' "another thread's work runs on the queue's thread; that thread has no queue"
ticks=$(printf '%s\n' "$out" | sed -n 's/^ticks=\([0-9]*\).*/\1/p')
if [ -n "$ticks" ] && [ "$ticks" -ge 6 ] && [ "$ticks" -le 12 ] && has 'stopped=1$'; then pass "a 50 ms timer ticks ~10 times in 530 ms ($ticks) and stops"
else fail "timer: ticks=${ticks:-none}"; fi
check '^once=1 running=0$' "a timer that does not repeat ticks once"
check '^second_current=1$' "a second queue for the same thread is refused"
check '^create_dedicated=0$' "a dedicated thread's queue"
check '^dedicated_ran=1 own_thread=1$' "runs its work on its own thread"
check '^shutdown=1 starting=1 completed=1 refused_after=1$' "ShutdownQueueAsync: ShutdownStarting, ShutdownCompleted, the action completes, no more work"
check '^on_dedicated_thread=1$' "DispatcherQueueController.CreateOnDedicatedThread"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
