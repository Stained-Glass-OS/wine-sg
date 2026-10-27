#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate: a notification-area icon's tooltip is not shown again after the icon
# is clicked, until the pointer has left it -- as on Windows. The pointer
# still over the icon re-armed it: it came up over the menu the click opened
# (Flameshot's, through sg-sni, 2026-10-27). Under Xvfb: the shell's
# taskbar, a probe's icon, xdotool's pointer.
#   1. hovered, its tooltip shows (the test works)
#   2. clicked, the pointer moving on it after: no tooltip
#   3. the pointer away and back: the tooltip shows again
#   WINE=... test/traytip-gate.sh
# Mutant: SG_MUTANT_TRAY_TIP_AFTER_CLICK (programs/explorer/systray.c) fails 2.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/traytip-gate.XXXXXX)
fails=0 XP= EP= PP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$PP" ] && kill "$PP" 2>/dev/null; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/traytip-probe.c" -luser32 -lshell32 || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & EP=$!
sleep 8
"$WINE" "$W/probe.exe" add 60 > "$W/add.out" 2>/dev/null & PP=$!
i=0; while ! grep -q '^rect=' "$W/add.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.25; i=$((i + 1)); done
set -- $(sed -n 's/^rect=//p' "$W/add.out" | tr -d '\r')
[ $# = 4 ] || { fail "the probe's icon is not in the taskbar: $(tr -d '\r' < "$W/add.out")"; echo "RESULT: FAIL"; exit 1; }
cx=$(( ($1 + $3) / 2 )) cy=$(( ($2 + $4) / 2 ))
tips() { "$WINE" "$W/probe.exe" tips 2>/dev/null | tr -d '\r' | sed -n 's/^tips=//p'; }
wiggle() { for k in 1 2 3 4 5 6; do xdotool mousemove $((cx + k % 2)) "$cy"; sleep 0.25; done; }

# 1. hovered
xdotool mousemove "$cx" "$cy"; wiggle; sleep 1.5
t1=$(tips)
[ "${t1:-0}" -ge 1 ] && pass "hovered, the icon's tooltip shows" || fail "hovered, no tooltip ($t1): test broken"
# 2. clicked, the pointer still moving on it
xdotool click 1; wiggle; wiggle; sleep 1.5
t2=$(tips)
[ "${t2:-1}" = 0 ] && pass "clicked, the pointer moving on it after: no tooltip" \
    || fail "clicked: the tooltip came back while the pointer was on the icon ($t2 shown; it covers a menu the click opens)"
# 3. away and back
xdotool mousemove 500 300; sleep 1
xdotool mousemove "$cx" "$cy"; wiggle; sleep 1.5
t3=$(tips)
[ "${t3:-0}" -ge 1 ] && pass "the pointer away and back, the tooltip shows again" || fail "away and back: no tooltip ($t3)"
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
