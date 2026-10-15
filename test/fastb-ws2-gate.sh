#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ws2_32: SIO_UDP_CONNRESET, DisconnectEx(TF_REUSE_SOCKET), WSACancelAsyncRequest, WSApSetPostRoutine,
# WPUCompleteOverlappedRequest, WSAJoinLeaf, WSAGetQOSByName (patch 3715)
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-ws2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-ws2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for p in udp reuse misc; do
    TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/$p.exe" "$HERE/fastb-ws2$p-probe.c" -lws2_32 -lmswsock \
        || { echo "FAIL  probe $p did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/*.exe "$WINEPREFIX/drive_c/"
rc=0
for p in udp reuse misc; do
    (cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" $p.exe 2>/dev/null </dev/null | tr -d '\r') > "$T/$p.out"
    sed "s/^/$p: /" "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
