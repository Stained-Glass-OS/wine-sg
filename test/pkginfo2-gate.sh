#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The package graph functions after GetCurrentPackageInfo (wine-sg 7754): GetCurrentPackageInfo2,
# GetCurrentPackageInfo3, GetPackageGraphRevisionId, OpenPackageInfoByFullName[ForUser], GetPackageInfo,
# GetPackageInfo2 and ClosePackageInfo. The Windows App SDK's runtime (Microsoft.WindowsAppRuntime.dll)
# looks the first ones up from the app model API sets in its DllMain on Windows 10 19H1 and later and
# stops its process (c0000409) when one is missing: Office's aimgr.exe (the Copilot "Describe what you'd
# like to draft" box) died so at every start, and Word failed its AI manager after 10 s. A deployed
# package record (HKCU\Software\Wine\AppModel\Packages) with a framework dependency and an external
# location; test/pkginfo2-probe.c in its folder and outside it, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/pkginfo2-gate.sh
# Mutants (dlls/kernelbase/version.c): SG_MUTANT_NO_PACKAGEINFO2, SG_MUTANT_PACKAGEINFO_CURRENT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pkginfo2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -municode -o "$T/probe64.exe" "$HERE/pkginfo2-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -municode -o "$T/probe32.exe" "$HERE/pkginfo2-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
mkdir -p "$C/pkg" "$C/ext" "$C/fw"
cp "$T/probe64.exe" "$T/probe32.exe" "$C/pkg/"
cp "$T/probe64.exe" "$T/probe32.exe" "$C/"
FULL=Sg.PkgInfo_1.2.3.4_x64__8wekyb3d8bbwe
FW=Sg.Framework_5.0.0.0_x64__8wekyb3d8bbwe
K="HKCU\\Software\\Wine\\AppModel\\Packages"
reg() { timeout -s KILL 60 "$WINE" reg "$@" >/dev/null 2>&1; }
reg add "$K\\$FULL" /v InstallLocation /t REG_SZ /d 'C:\pkg' /f
reg add "$K\\$FULL" /v ExternalLocation /t REG_SZ /d 'C:\ext' /f
reg add "$K\\$FULL" /v Dependencies /t REG_MULTI_SZ /d "$FW" /f
reg add "$K\\$FW" /v InstallLocation /t REG_SZ /d 'C:\fw' /f
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
for b in 64 32; do
    out=$(run 'C:\pkg\probe'$b'.exe' current)
    n=$(printf '%s\n' "$out" | grep -c '^Export\..*=1$')
    [ "$n" = 7 ] && pass "$b: all seven exports are found where the Windows App SDK looks" ||
        fail "$b: exports: $(printf '%s\n' "$out" | grep '^Export' | tr '\n' ' ')"
    expect "$out" InstallCount 2 "$b: GetCurrentPackageInfo2: the package and its framework"
    expect "$out" Install0 "$FULL|Sg.PkgInfo_8wekyb3d8bbwe|C:\\pkg|0" "$b: the package first, at its install location"
    expect "$out" Install1 "$FW|Sg.Framework_8wekyb3d8bbwe|C:\\fw|0x80001" "$b: then the framework (static)"
    expect "$out" HeadExternalCount 1 "$b: PACKAGE_FILTER_HEAD: the package alone"
    expect "$out" HeadExternal0 "$FULL|Sg.PkgInfo_8wekyb3d8bbwe|C:\\ext|0" "$b: PackagePathType_EffectiveExternal: its external location"
    expect "$out" Info3Size 0x8007007a "$b: GetCurrentPackageInfo3 asks for its size (an HRESULT)"
    expect "$out" Info3 0x00000000 "$b: and fills it"
    expect "$out" Info3Count 2 "$b: with the graph"
    expect "$out" Generation "0x00000000 1" "$b: PackageInfo3Type_PackageInfoGeneration: the graph's generation"
    expect "$out" Revision 1 "$b: GetPackageGraphRevisionId"
    out=$(run 'C:\probe'$b'.exe' current)
    expect "$out" InstallRet 15700 "$b: an unpackaged program: APPMODEL_ERROR_NO_PACKAGE"
    expect "$out" Info3Size 0x80073d54 "$b: and from GetCurrentPackageInfo3 as an HRESULT"
    out=$(run 'C:\probe'$b'.exe' open "$FULL")
    expect "$out" Open 0 "$b: OpenPackageInfoByFullName of a deployed package (from outside it)"
    expect "$out" SizeRet 122 "$b: GetPackageInfo asks for its size"
    expect "$out" InfoCount 2 "$b: GetPackageInfo: that package's graph"
    expect "$out" Info1 "$FW|Sg.Framework_8wekyb3d8bbwe|C:\\fw|0x80001" "$b: with its framework"
    expect "$out" HeadEffective0 "$FULL|Sg.PkgInfo_8wekyb3d8bbwe|C:\\pkg|0" "$b: GetPackageInfo2 effective path of the head"
    expect "$out" Close 0 "$b: ClosePackageInfo"
    out=$(run 'C:\probe'$b'.exe' open Sg.Missing_1.0.0.0_x64__8wekyb3d8bbwe)
    expect "$out" Open 1168 "$b: a package that is not deployed: ERROR_NOT_FOUND"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
