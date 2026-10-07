#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A person's task host (patch 1173: "rundll32 schedsvc.dll,SgUserTaskHost",
# started by the shell at sign-in) keeps nothing alive (patch 1444). It was
# an ordinary process with a window: the shell's desktop never closed, and
# "wineserver -w" -- a script's, a prefix's first start (sg-shell's
# start-check hung on it for 25 minutes) -- never returned. Now:
#   1. the shell starts the host (it is running)
#   2. with the shell closed, the host does not keep the wineserver: the
#      wineserver ends within 30 s ("wineserver -w" returns)
#   3. and the host is gone with it
# Under Xvfb.  WINE=... test/taskhost-gate.sh
# Mutant: SG_MUTANT_TASKHOST_KEEPS_SESSION (schedsvc taskrun.c) fails 2 and 3.
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
W=$(mktemp -d /var/tmp/taskhost-gate.XXXXXX)
fails=0 XP= EP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
DPY=$((640 + $$ % 50)); while [ -e "/tmp/.X$DPY-lock" ]; do DPY=$((DPY + 1)); done
cleanup() { set +e; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

"$WINE" explorer /desktop=shell,800x600 >/dev/null 2>&1 & EP=$!
host() { for p in $(pgrep -u "$(id -u)" -f 'SgUserTaskHost'); do tr '\0' '\n' < /proc/$p/environ 2>/dev/null | grep -qx "WINEPREFIX=$WINEPREFIX" && echo "$p"; done; }
i=0; while [ -z "$(host)" ] && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
[ -n "$(host)" ] && pass "the shell starts the person's task host" || fail "no task host started"
# the shell closed (as a sign-out ends it): nothing of the person's is left
"$WINE" cmd /c exit 0 >/dev/null 2>&1   # a user process that comes and goes
kill "$EP" 2>/dev/null; wait "$EP" 2>/dev/null; EP=
timeout 30 "$WINESERVER" -w; rc=$?
[ "$rc" = 0 ] && pass "the host keeps no wineserver alive: wineserver -w returned" \
    || fail "wineserver -w still waiting after 30 s (the task host holds it)"
sleep 1
[ -z "$(host)" ] && pass "and the host is gone with the session" || fail "the task host is still running"
echo "taskhost-gate: $fails failure(s)"
[ "$fails" = 0 ]
