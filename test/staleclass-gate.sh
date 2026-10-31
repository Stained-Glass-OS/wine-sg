#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A local server that ended without revoking its class (patches/sg/6225): rpcss drops the class
# when its process ends, so activation starts the class's LocalServer32 again instead of failing
# with RPC_S_SERVER_UNAVAILABLE on the dead server's object (OneDrive and an exited FileCoAuth).
# test/staleclass-probe.c (client and server in one).
#   WINE=/opt/wine-sg/bin/wine test/staleclass-gate.sh
# Mutant: SG_MUTANT_RPCSS_KEEPS_DEAD_SERVERS (programs/rpcss/rpcss_main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-staleclass.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/staleclass-probe.exe" "$HERE/staleclass-probe.c" -municode -lole32 -luuid -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/staleclass-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" staleclass-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^registered=1$' "a server registers the class"
chk '^killed=1$' "and is killed without revoking it"
chk '^activate=00000000 fast=1$' "activation starts the server again (not the dead one's object)"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
