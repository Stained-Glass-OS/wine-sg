#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0439: signing out ends the shell. ExitWindowsEx(EWX_LOGOFF)
# -- Start's Sign out, the Ctrl+Alt+Del screen's, "shutdown /l" -- closed the
# session's programs and left the desktop process (explorer /desktop=shell)
# running: no Start menu, no taskbar, and the session never ended (it ends
# when the shell exits cleanly). Under Xvfb: a shell, then "shutdown /l".
#   WINE=... test/logoff-gate.sh
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/logoff-gate.XXXXXX)
fails=0 XP= EP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" SG_POWERCTL=/bin/true
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# as in a session: programs started outside the shell join its desktop
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w

"$WINE" explorer /desktop=shell,800x600 >/dev/null 2>&1 & EP=$!
sleep 8
"$WINE" notepad >/dev/null 2>&1 &
sleep 4
kill -0 "$EP" 2>/dev/null && pass "the shell is running" || { fail "the shell did not start"; exit 1; }
"$WINE" shutdown /l >/dev/null 2>&1
rc=-1
for _ in $(seq 1 40); do kill -0 "$EP" 2>/dev/null || { wait "$EP"; rc=$?; break; }; sleep 0.5; done
[ "$rc" = 0 ] && { EP=; pass "signing out ends the shell, cleanly (exit 0): the session ends"; } \
    || fail "the shell is still running 20 s after signing out (rc $rc)"
"$WINESERVER" -w 2>/dev/null &
echo "logoff-gate: $fails failure(s)"
[ "$fails" = 0 ]
