#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0442: a notification-area icon that changes to one with
# transparent pixels no longer shows the old icon through them (the network
# icon's monitor showed through its new Wi-Fi fan). Under Xvfb: the shell's
# taskbar, and a probe that swaps a red square for a white dot.
#   WINE=... test/trayicon-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/trayicon-gate.XXXXXX)
fails=0 XP= EP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/trayicon-probe.c" -luser32 -lgdi32 -lshell32 || { fail "probe did not build"; exit 1; }
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
grep -q '^icon=1' <<<"$out" && pass "the icon is in the notification area" || fail "no tray icon: $out"
set -- $(grep '^icon=1' <<<"$out" | sed 's/.*red=//; s/ of / /')
[ "${1:-99}" = 0 ] && pass "after the change, nothing of the old red icon shows" || fail "the old icon shows through: ${1:-?} red pixels of ${2:-?}"
echo "trayicon-gate: $fails failure(s)"
[ "$fails" = 0 ]
