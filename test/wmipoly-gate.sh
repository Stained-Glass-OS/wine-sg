#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemprox: a WQL query of a class gives the instances of its subclasses too (6054). SELECT * FROM
# CIM_LogicalDevice gave nothing and CIM_Processor gave a copy of Win32_Processor's rows as CIM_Processor:
#  - CIM_Processor gives the Win32_Processor instances, as Win32_Processor;
#  - CIM_LogicalDevice gives processors, logical disks and more; a WHERE clause applies to every subclass;
#  - CIM_Setting gives Win32_Environment; Win32_BaseService gives the Win32_Service instances;
#  - a leaf class gives only itself; a CIM_ class's path (GetObject) is the class, not a subclass instance.
#   WINE=/opt/wine-sg/bin/wine test/wmipoly-gate.sh   (mutant SG_MUTANT_WMI_NOT_POLYMORPHIC)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmipoly.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/wmipoly.js" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\wmipoly.js' 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect cim_processor "true Win32_Processor" "CIM_Processor gives the processors, as Win32_Processor"
expect logical_device "1 1 1" "CIM_LogicalDevice gives processors, logical disks and more"
expect logical_disk "1 Win32_LogicalDisk" "a WHERE clause applies to the subclasses' instances"
expect setting 1 "CIM_Setting gives Win32_Environment"
expect base_service "true 1" "Win32_BaseService gives the services"
expect leaf Win32_Processor "a leaf class gives only itself"
expect class_def CIM_Processor "a CIM_ class's definition is its own"
expect done 1 "the script ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
