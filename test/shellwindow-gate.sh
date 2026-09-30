#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0526: GetShellWindow() on the shell's desktop is
# explorer's, as on Windows (it was NULL: explorer set it only on a desktop
# named "Default"; Stained Glass OS's is "shell"). Installers find the
# signed-in user by it to run what they installed as that user.
#   WINE=... test/shellwindow-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/shellwindow-gate.XXXXXX)
fails=0 XP= EP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/shellwindow-probe.c" -luser32 -lgdi32 -lshell32 || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & EP=$!
sleep 8
out=$(timeout 60 "$WINE" "$W/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { sed -n "s/^$1=//p" <<<"$out"; }
[ "$(v desktop)" = shell ] && pass "the probe runs on the shell's desktop" || fail "desktop: $(v desktop)"
[ "$(v shell)" = 1 ] && pass "GetShellWindow() is set" || fail "GetShellWindow() is NULL"
[ "$(v owner)" = explorer.exe ] && pass "and is explorer's" || fail "owner: '$(v owner)'"
echo "shellwindow-gate: $fails failure(s)"
[ "$fails" = 0 ]
