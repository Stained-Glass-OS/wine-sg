#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The InkDisp class (patches/sg/1521): the ink object of the Tablet PC
# automation API, which every Windows client edition registers. OneNote looks
# its class up at start; without it, OneNote said "You'll need to install the
# Desktop Experience before you start OneNote" (the Server feature that brings
# ink) and quit. The class is registered by wineboot in both views, creates,
# keeps its Dirty flag, clones to a fresh ink and answers IDispatch.
#
#   WINE=/opt/wine-sg/bin/wine test/inkdisp-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null \
    || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-inkdisp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/inkdisp-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/inkdisp-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
WANT="key=1 create=0 dirty0=0 dirty1=-1 clone=0 clonedirty=0 disp=0"
for a in 64 32; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r')
    echo "      $a: $O"
    [ "$O" = "$WANT" ] && pass "$a-bit: InkDisp is registered, creates, keeps Dirty, clones, answers IDispatch" \
        || fail "$a-bit: $O"
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
