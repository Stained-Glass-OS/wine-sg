#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Long paths (patches/sg/9755). Paths longer than MAX_PATH work in Wine without the \\?\ prefix, but
# the machine said nothing about it: no LongPathsEnabled policy (MusicBrainz Picard: "Failed reading
# LongPathsEnabled from registry", and it keeps file names short), and no ntdll!RtlAreLongPathsEnabled,
# which Windows 10 has. The probe (test/longpathpolicy-probe.c), 64-bit and 32-bit, checks:
#   HKLM\System\CurrentControlSet\Control\FileSystem LongPathsEnabled is 1 (wine.inf)
#   RtlAreLongPathsEnabled exists and says yes
#   a file more than 300 characters deep is made without \\?\ (what the policy promises)
#   WINE=/opt/wine-sg/bin/wine test/longpathpolicy-gate.sh
# Mutant (ntdll/path.c): SG_MUTANT_NO_LONG_PATHS (the policy line is inf data: the gate fails on 10.0-1151).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-longpathpolicy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/longpathpolicy-probe.c" -ladvapi32 &&
i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/longpathpolicy-probe.c" -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
for b in 64 32; do
    out=$(timeout 120 "$WINE" "$T/probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '      %s\n' $(printf '%s\n' "$out" | tr ' ' '=')
    l() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
    [ "$(l POLICY)" = 1 ] && pass "$b-bit: the machine's LongPathsEnabled policy is on" || fail "$b-bit: policy '$(l POLICY)'"
    [ "$(l RTL)" = 1 ] && pass "$b-bit: RtlAreLongPathsEnabled says yes" || fail "$b-bit: RtlAreLongPathsEnabled '$(l RTL)'"
    set -- $(l LONG)
    [ "${1:-}" = ok ] && [ "${2:-0}" -gt 300 ] && pass "$b-bit: a file $2 characters deep, no \\\\?\\ prefix" || fail "$b-bit: long path '$(l LONG)'"
done
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
