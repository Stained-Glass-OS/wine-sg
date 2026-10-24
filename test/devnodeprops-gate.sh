#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Device properties Windows derives from a device node (6086). CM_Get_DevNode_PropertyW and
# SetupDiGetDevicePropertyW only knew properties stored under the device's Properties key, so
# DEVPKEY_Device_InstanceId (GTK 4 asks it for every input device: Inkscape 1.4 warned "Failed to get device
# node property {78C34FC8-...},256: Element not found") and the DEVPKEY_Device_* forms of the SPDRP_* registry
# properties (description, hardware IDs, class GUID, friendly name...) were "not found". Now they come from the
# device node: the instance ID, and the registry property with pid = SPDRP + 2, typed as DEVPROP_TYPE_STRING /
# STRING_LIST / UINT32 / GUID.
#   WINE=/opt/wine-sg/bin/wine test/devnodeprops-gate.sh
# Mutants: SG_MUTANT_DEVPROP_NO_INSTANCEID, SG_MUTANT_DEVPROP_NO_SPDRP (setupapi).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-devnodeprops.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/devnodeprops-probe.c" -lsetupapi -lcfgmgr32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect device 1 "the prefix has a device with a description and hardware IDs"
expect instance_id "0 type 0x12 same 1" "DEVPKEY_Device_InstanceId: a string, the device's instance ID"
expect instance_id_size 1 "its size, terminator included"
expect small_buffer "26 needs 1" "too small a buffer: CR_BUFFER_SMALL and the size needed"
expect desc "0 type 0x12 same 1" "DEVPKEY_Device_DeviceDesc is SPDRP_DEVICEDESC"
expect hwids "0 type 0x2012 same 1" "DEVPKEY_Device_HardwareIds is SPDRP_HARDWAREID, a string list"
expect classguid "1 type 0xd size 16 same 1" "DEVPKEY_Device_ClassGuid through SetupDiGetDevicePropertyW: a GUID"
expect friendly 1 "DEVPKEY_Device_FriendlyName: the value or CR_NO_SUCH_VALUE"
expect done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
