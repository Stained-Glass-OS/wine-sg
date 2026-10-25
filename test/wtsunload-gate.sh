#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wtsapi32's session thread keeps the library loaded (patches/sg/6531): a program that registers
# for session notifications, unregisters and frees wtsapi32 (MPC-HC at start) no longer crashes
# when the thread wakes up in unloaded code (test/wtsunload-probe.c; no compositor, so the thread
# takes its retry path every 5 s).
#   WINE=/opt/wine-sg/bin/wine test/wtsunload-gate.sh
# Mutant: SG_MUTANT_WTS_NO_MODULE_REF (dlls/wtsapi32/wtsapi32.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wtsunload.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wtsunload-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
if printf '%s\n' "$out" | grep -q "^round0 register=1 unregister=1"; then pass "a window registered and unregistered"
else fail "register/unregister ($(printf '%s\n' "$out" | grep '^round0' || echo none))"; fi
check survived "freed wtsapi32, the program keeps running past the session thread's retries (twice)"
check done     "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
