#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Foundation.Deferral (patches/sg/6209): DeferralFactory did not exist; OneDrive (React
# Native for Windows) retried making deferrals in a loop. Complete runs the handler once, Close
# lets it go unrun.
#   WINE=/opt/wine-sg/bin/wine test/deferral-gate.sh
# Mutant (dlls/wintypes/main.c): SG_MUTANT_NO_DEFERRAL_FACTORY.
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
T=$(mktemp -d /var/tmp/sg-deferral.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/deferral-probe.exe" "$HERE/deferral-probe.c" -lruntimeobject -lole32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/deferral-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -q -- "$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2"; fi; }
check '^factory=00000000$' "Windows.Foundation.Deferral's factory"
check '^create=00000000 held=1$' "Create keeps its handler"
check '^complete=1 released=1$' "Complete runs the handler once, then lets it go"
check '^closed=1,0 released=1$' "Close lets the handler go unrun"
check '^no_handler=80070057$' "no handler: E_INVALIDARG"
check '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
