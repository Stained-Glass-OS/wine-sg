#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wminet_utils.dll, which .NET's System.Management loads from the .NET Framework
# folder (patches/sg/5943): a new prefix has it in Framework64 and Framework
# (v4.0.30319), it has every function System.Management looks up, and through it
# a WQL query of ROOT\CIMV2 returns Win32_OperatingSystem with its Caption and
# properties. Without it every use of System.Management threw at its first type
# initializer (DYMO Connect's launcher crashed at start). 64- and 32-bit.
# An event watcher's sink gets an IWbemObjectSink stub that hands it each object and the status
# (ManagementEventWatcher.Start() threw InvalidCastException without it).
# Mutants: SG_MUTANT_WMINET_NO_QUERY (ExecQueryWmi fails), SG_MUTANT_WMINET_SELF_STUB (the sink itself
# handed back) fail it.
#
#   WINE=/opt/wine-sg/bin/wine test/wminetutils-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wminetutils.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for b in 64 32; do
    [ $b = 64 ] && cc=x86_64-w64-mingw32-gcc || cc=i686-w64-mingw32-gcc
    $cc -O1 -o "$T/probe$b.exe" "$HERE/wminetutils-probe.c" -lole32 -loleaut32 -luuid -lwbemuuid || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for d in Framework64 Framework; do
    [ -f "$WINEPREFIX/drive_c/windows/Microsoft.NET/$d/v4.0.30319/wminet_utils.dll" ] \
        && printf 'PASS  a new prefix has Microsoft.NET\\%s\\v4.0.30319\\wminet_utils.dll\n' "$d" \
        || { echo "FAIL  no wminet_utils.dll in $d"; rc=1; }
done
for b in 64 32; do
    out=$(timeout -s KILL 120 "$WINE" "$T/probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      ($b) /"
    case "$out" in *ALL-OK*) echo "PASS  ($b-bit) System.Management's functions are there, and a WQL query reads Win32_OperatingSystem" ;;
    *) echo "FAIL  ($b-bit) wminet_utils.dll does not do what System.Management needs"; rc=1 ;; esac
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
