#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# traffic.dll (6065, C3): the Traffic Control API needs the packet classifier device and a QoS packet scheduler,
# which this system does not have. TcRegisterClient checked nothing and said ERROR_CALL_NOT_IMPLEMENTED, the other
# exports were stubs that crash a caller. Now: NULL arguments ERROR_INVALID_PARAMETER, a wrong TCI version
# ERROR_INCOMPATIBLE_TCI_VERSION, and otherwise ERROR_OPEN_FAILED ("Traffic control failed to open a system
# device", the documented failure) with no handle; calls on a client, interface or flow handle answer
# ERROR_INVALID_HANDLE, and a flow name ERROR_WMI_INSTANCE_NOT_FOUND.
#   WINE=/opt/wine-sg/bin/wine test/tcnone-gate.sh   (mutant SG_MUTANT_TC_REGISTER_OK)
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
T=$(mktemp -d /var/tmp/sg-tcnone.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/tcnone-probe.exe" "$HERE/tcnone-probe.c" || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/tcnone-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" tcnone-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect exports 1 "the functions are exported"
expect RegisterNull 87 "TcRegisterClient without a function list: ERROR_INVALID_PARAMETER"
expect RegisterNoNotify 87 "without a notify handler: ERROR_INVALID_PARAMETER"
expect RegisterVersion "7501 1" "a wrong TCI version: ERROR_INCOMPATIBLE_TCI_VERSION, no handle"
expect Register "110 1" "no traffic control device: ERROR_OPEN_FAILED, no handle"
expect Deregister 6 "TcDeregisterClient: ERROR_INVALID_HANDLE"
expect EnumInterfaces 6 "TcEnumerateInterfaces: ERROR_INVALID_HANDLE"
expect OpenInterface 6 "TcOpenInterfaceW: ERROR_INVALID_HANDLE"
expect QueryFlow 4201 "TcQueryFlowW of a flow name: ERROR_WMI_INSTANCE_NOT_FOUND"
expect QueryFlowNull 87 "and of no name: ERROR_INVALID_PARAMETER"
expect DeleteFlow 6 "TcDeleteFlow: ERROR_INVALID_HANDLE"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
