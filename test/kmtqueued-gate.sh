#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# win32u NtGdiDdDDISetQueuedLimit (6063, C2: D3DKMT validation): it returned STATUS_NOT_IMPLEMENTED. A device's
# queued-present limit is set and read back (D3DKMT_SET/GET_QUEUEDLIMIT_PRESENT), per device; a device handle
# that was never created or was destroyed, an unknown type and NULL give STATUS_INVALID_PARAMETER. Under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/kmtqueued-gate.sh   (mutant SG_MUTANT_QUEUEDLIMIT_ANY_DEVICE)
#   WINE_INCLUDE=<wine source include dir> when the installed headers lack ddk/d3dkmthk.h
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
DPY="${KMTQ_DPY:-687}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb xdpyinfo x86_64-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kmtqueued.XXXXXX)
# shellcheck disable=SC2317  # invoked via trap
cleanup() { [ -n "$XP" ] && kill "$XP" 2>/dev/null; WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
inc="-idirafter ${WINE_INCLUDE:-/nonexistent} -idirafter $(dirname "$WINE")/include/wine/windows -idirafter /opt/wine-sg/include/wine/windows"
# shellcheck disable=SC2086  # the include list is words
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -mwindows $inc -o "$T/probe.exe" "$HERE/kmtqueued-probe.c" || { fail "the probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 90 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
out=$(tr -d '\r' <"$T/out.raw")
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect OpenAdapter 0 "an adapter for the display"
expect CreateDevice 0 "a device on it"
expect GetInitial "0 0" "no limit is set at first"
expect Set 0 "D3DKMT_SET_QUEUEDLIMIT_PRESENT sets one"
expect Get "0 2" "D3DKMT_GET_QUEUEDLIMIT_PRESENT reads it back"
expect GetOther "0 0" "another device keeps its own"
expect BadDevice 0xc000000d "a device that was never created: STATUS_INVALID_PARAMETER"
expect BadType 0xc000000d "an unknown type: STATUS_INVALID_PARAMETER"
expect Null 0xc000000d "NULL: STATUS_INVALID_PARAMETER"
expect Destroyed 0xc000000d "a destroyed device: STATUS_INVALID_PARAMETER"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
