#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Proxies for IObjectArray and IObjectCollection (wine-sg 7781): actxprxy has them, as on Windows.
# Office hands object arrays to other apartments and logged "CoMarshalInterface failed" for
# {92ca9dcd-...} without them. test/objarrproxy-probe.c: an STA's object array called from an MTA
# runs on its own thread; shell32's EnumerableObjectCollection crosses apartments.
#   WINE=/opt/wine-sg/bin/wine test/objarrproxy-gate.sh
# Before 7781 it fails at MarshalArray=0x80004002.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-objarrproxy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/objarrproxy-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect MarshalArray 0x00000000 "an IObjectArray marshals (it has a proxy)"
expect UnmarshalArray 0x00000000 "another apartment unmarshals it"
expect GetCount "0x00000000 2" "and calls it"
expect CalledOnOwnThread 1 "on its own (STA) thread"
expect CreateCollection 0x00000000 "shell32's EnumerableObjectCollection is made"
expect MarshalCollection 0x00000000 "an IObjectCollection marshals"
expect UnmarshalCollection 0x00000000 "and unmarshals in another apartment"
expect AddObject 0x00000000 "AddObject through the proxy"
expect CollectionCount 1 "the collection has the object"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
