#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows 10 22H2 (patches/sg/0175) and DXGIDeclareAdapterRemovalSupport
# (0176): what Paint.NET 5 checked first. Also that an existing prefix
# moves to 19045 on `wineboot -u` (wine.inf rewrites the values).
#
#   WINE=/opt/wine-sg/bin/wine test/winver-gate.sh
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
T=$(mktemp -d /var/tmp/sg-winver.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/winver-probe.exe" "$HERE/winver-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/winver-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() { if printf '%s\n' "$out" | grep -qx "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi; }
expect "version=10.0.19045" "RtlGetVersion: Windows 10 build 19045 (22H2)"
expect "CurrentBuild=19045" "the registry's CurrentBuild"
expect "DisplayVersion=22H2" "DisplayVersion 22H2"
expect "UBR=6456" "and an update revision (UBR)"
expect "declare=0,0x887a0036" "DXGIDeclareAdapterRemovalSupport: S_OK, then DXGI_ERROR_ALREADY_EXISTS"
# an existing prefix that said 19043 is moved on by wineboot -u
"$WINE" reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion' /v CurrentBuildNumber /d 19043 /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion' /v CurrentBuild /d 19043 /f >/dev/null 2>&1
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/winver-probe.exe" 2>/dev/null | tr -d '\r')
expect "version=10.0.19045" "an existing 19043 prefix reports 19045 after wineboot -u"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
