#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0522: Shell_NotifyIconGetRect gives the screen rectangle of
# a notification-area icon, found by its window and id or by its GUID, as on
# Windows. Microsoft OneDrive places its flyout by its icon; the stub's
# E_NOTIMPL left a click on the icon doing nothing. Under Xvfb: the shell's
# taskbar, and a probe with two icons.
#   WINE=... test/notifyiconrect-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DPY=${DPY:-$((700 + $$ % 200))}
W=$(mktemp -d /var/tmp/notifyiconrect-gate.XXXXXX)
fails=0 XP= EP=
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
for t in Xvfb xdpyinfo "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
cleanup() { set +e; [ -n "$EP" ] && kill "$EP" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP"; sleep 1; rm -rf "$W" "/tmp/.X${DPY}-lock"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/notifyiconrect-probe.c" -luser32 -lgdi32 -lshell32 || { fail "probe did not build"; exit 1; }
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
[ "$(v add)" = "1 1" ] && pass "two icons added" || fail "add: $(v add)"
case "$(v byid)" in "00000000 tray=1 inbar=1 size=20x20"|"00000000 tray=1 inbar=1 size="[1-9]*) pass "by window and id: S_OK, the icon's own rectangle, in the taskbar";; *) fail "by id: $(v byid)";; esac
[ "$(v byguid)" = "00000000 tray=1 distinct=1" ] && pass "by GUID: S_OK, the other icon's rectangle" || fail "by guid: $(v byguid)"
[ "$(v unknown)" = "80004005" ] && pass "an icon that is not there: E_FAIL" || fail "unknown: $(v unknown)"
[ "$(v badsize)" = "80070057" ] && pass "a wrong cbSize: E_INVALIDARG" || fail "badsize: $(v badsize)"
[ "$(v deleted)" = "80004005" ] && pass "a deleted icon: E_FAIL" || fail "deleted: $(v deleted)"
echo "notifyiconrect-gate: $fails failure(s)"
[ "$fails" = 0 ]
