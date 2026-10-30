#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A DispatcherQueue can be held weakly (patches/sg/6202).
#
# Programs built with C++/WinRT keep their thread's DispatcherQueue as a weak
# reference (React Native for Windows does, in OneDrive): the queue had no
# IWeakReferenceSource and OneDrive crashed as its window was made.
#
#   WINE=/opt/wine-sg/bin/wine test/dqweak-gate.sh
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
T=$(mktemp -d /var/tmp/sg-dqweak.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/dqweak-probe.exe" "$HERE/dqweak-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/dqweak-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -q -- "$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2"; fi; }
check '^source=1$' "a DispatcherQueue gives IWeakReferenceSource"
check '^weak=1$' "GetWeakReference"
check '^live_queue=1$' "while the queue lives its weak reference resolves to it"
check '^live_queue2=1$' "and to its other interfaces"
check '^second_weak=1$' "a second weak reference resolves too"
check '^gone=1$' "once the queue is shut down and let go, it resolves to nothing (S_OK, null)"
check '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
