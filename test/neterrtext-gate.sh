#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Network drive errors in Windows' words (1101). NET USE prints "System error
# N has occurred." and the system's message for N; Wine's were its own short
# ones -- "Bad network path.", "Access denied.", "Already assigned." (s14
# regression walk, 2026-10-06: NET USE to a server that is not there; to a
# letter in use). The messages for the network drive errors are Windows':
# this gate asks FormatMessage for each.
#
#   WINE=/opt/wine-sg/bin/wine test/neterrtext-gate.sh
#   (mutant: dlls/kernelbase/winerror.mc with the old texts back)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-neterrtext.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/neterrtext-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" "$T/probe.exe" 5 53 67 85 86 1203 1219 1222 1326 2250 2>/dev/null | tr -d '\r' > "$T/out"
while IFS='|' read -r code want; do
    got=$(sed -n "s/^$code|//p" "$T/out")
    [ "$got" = "$want" ] && pass "$code: $got" || fail "$code: '$got' (want '$want')"
done <<'W'
5|Access is denied.
53|The network path was not found.
67|The network name cannot be found.
85|The local device name is already in use.
86|The specified network password is not correct.
1203|The network path was either typed incorrectly, does not exist, or the network provider is not currently available. Please try retyping the path or contact your network administrator.
1219|Multiple connections to a server or shared resource by the same user, using more than one user name, are not allowed. Disconnect all previous connections to the server or shared resource and try again.
1222|The network is not present or not started.
1326|The user name or password is incorrect.
2250|This network connection does not exist.
W
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
