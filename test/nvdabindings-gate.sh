#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The functions NVDA binds by name at start are there (patches/sg/5941), 64-
# and 32-bit (NVDA is 32-bit): kernel32.GetConsoleSelectionInfo, user32's
# Register/UnregisterPointerInputTarget and GhostWindowFromHungWindow, oleacc's
# AccSetRunningUtilityState and AccNotifyTouchInteraction. NVDA's installer
# stopped at the first missing one, and SG Store installed nothing.
#
#   WINE=/opt/wine-sg/bin/wine test/nvdabindings-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-nvdabindings.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/nvdabindings-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/nvdabindings-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for b in 64 32; do
    out=$(timeout -s KILL 60 "$WINE" "$T/probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      ($b) /"
    case "$out" in *ALL-OK*) echo "PASS  ($b-bit) what NVDA binds is there and answers as on Windows" ;;
    *) echo "FAIL  ($b-bit) a function NVDA binds is missing or wrong"; rc=1 ;; esac
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
