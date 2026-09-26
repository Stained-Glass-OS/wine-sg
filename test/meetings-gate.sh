#!/bin/sh
# What the meeting programs need at start (patches/sg/0425-0426), found by
# the compat suite's Zoom and Teams entries:
#
#  - SetThreadpoolTimerEx and SetThreadpoolWaitEx, which return whether a
#    timer or wait was pending (0425; Zoom.exe died at start on the stub);
#  - Windows.ApplicationModel.LimitedAccessFeatures, whose TryUnlockFeature
#    answers "unavailable" (0426; ms-teams.exe quit with 3 without it).
#
#   WINE=/opt/wine-sg/bin/wine test/meetings-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-meetings.XXXXXX)
# a scratch HOME and no menu/desktop integration: a prefix links its
# Desktop, Documents... to $HOME's
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop" WINEDLLOVERRIDES="winemenubuilder.exe=d"
mkdir -p "$HOME"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/meetings-probe.exe" "$HERE/meetings-probe.c" \
    -lruntimeobject || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/meetings-probe.exe" "$WINEPREFIX/drive_c/"

out=$(timeout -s KILL 120 "$WINE" 'C:\meetings-probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed '/^$/d; s/^/      /'
has() { printf '%s\n' "$out" | grep -q "^$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" | head -1 || echo none))"; fi; }

check 'timer_ex_exported=1' "kernel32 exports SetThreadpoolTimerEx"
check 'timer_ex_pending=1'  "SetThreadpoolTimerEx tells whether a timer was pending (set, replace, cancel)"
check 'timer_ex_fires=1'    "a timer set with SetThreadpoolTimerEx fires once"
check 'wait_ex_exported=1'  "kernel32 exports SetThreadpoolWaitEx"
check 'wait_ex_pending=1'   "SetThreadpoolWaitEx tells whether a wait was pending"
check 'laf_factory=1'       "Windows.ApplicationModel.LimitedAccessFeatures activates"
check 'laf_try_unlock=1'    "TryUnlockFeature returns a result"
check 'laf_feature_id=1'    "the result names the feature asked for"
check 'laf_unavailable=1'   "the feature is unavailable"
check 'done=1'              "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
