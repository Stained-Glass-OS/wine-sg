#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemprox: IWbemServices::CreateClassEnum and CreateClassEnumAsync (6040). Both returned WBEM_E_FAILED,
# so SWbemServices.SubclassesOf, Get-WmiObject -List and WMI browsers listed no class.
#   - all classes of a namespace as class objects, deep or shallow (WBEM_FLAG_SHALLOW), from a superclass
#     (CIM_LogicalDisk's Win32_LogicalDisk) or the top; an unknown superclass is WBEM_E_INVALID_CLASS
#   - the classes an application stored, also in another process; not the deleted ones
#   - the definitions only: no table is filled (CIM_DataFile's would list the disk)
#   - VBScript's SubclassesOf
#   WINE=/opt/wine-sg/bin/wine test/wmienum-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wmienum.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O2 -o "$T/probe-$a.exe" "$HERE/wmienum-probe.c" -municode -lole32 -loleaut32 -lwbemuuid 2>/dev/null ||
        { echo "FAIL  the $a probe did not build"; exit 1; }
done

mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
for a in x86_64 i686; do
    for step in define list; do
        echo "== $a $step"
        timeout -s KILL 120 "$WINE" "$T/probe-$a.exe" $step >"$T/out.raw" 2>/dev/null </dev/null
        tr -d '\r' <"$T/out.raw" >"$T/out"
        cat "$T/out"
        grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
    done
done
cat > "$WINEPREFIX/drive_c/sub.vbs" <<'VBS'
Set svc = GetObject("winmgmts:\\.\root\cimv2")
n = 0 : os = False
For Each c In svc.SubclassesOf()
  n = n + 1
  If c.Path_.Class = "Win32_OperatingSystem" Then os = True
Next
s = ""
For Each c In svc.SubclassesOf("CIM_LogicalDisk")
  s = s & c.Path_.Class
Next
WScript.Echo "count=" & (n > 40) & " os=" & os & " disk=" & s
VBS
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\sub.vbs' 2>/dev/null </dev/null | tr -d '\r')
if [ "$out" = "count=True os=True disk=Win32_LogicalDisk" ]; then echo "ok    VBScript SubclassesOf lists the classes"
else echo "FAIL  VBScript SubclassesOf: $out"; rc=1; fi
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
