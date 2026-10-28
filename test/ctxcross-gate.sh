#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# combase IContextCallback::ContextCallback from another apartment (patches/sg/6180): an MTA thread
# entering an STA thread's context runs the function on that thread from its message loop (C++/WinRT's
# co_await of an apartment_context: Windows Terminal's UI updates); an STA thread entering the MTA's
# context runs it on an MTA thread; the function's result comes back. test/ctxcross-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/ctxcross-gate.sh
# Mutant (dlls/combase/combase.c): SG_MUTANT_CTXCALLBACK_NO_CROSS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ctxcross.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/ctxcross-probe.exe" "$HERE/ctxcross-probe.c" -lole32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/ctxcross-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" ctxcross-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^context=00000000$' "the STA thread's object context"
chk '^sta=00000001,42,1,1$' "an MTA thread enters the STA's context: run on the STA thread, its result returned"
chk '^mta=00000001,100,1,1$' "the STA thread enters the MTA's context: run on an MTA thread"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
