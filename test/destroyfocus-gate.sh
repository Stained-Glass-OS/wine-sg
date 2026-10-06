#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A child window being destroyed while one of its own children has the
# keyboard focus gives the focus to its parent (patches/sg/0960), as the
# destroyed window itself would. Wine left the focus on nothing: AnyDesk
# focuses the Cancel button of its "Connecting" panel, destroys the panel
# once connected, and the keys typed at the remote screen went nowhere until
# the window was clicked away from and back.
#
#   WINE=/opt/wine-sg/bin/wine test/destroyfocus-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-destroyfocus.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/destroyfocus-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# windows without a display: Wine's null display driver
env DISPLAY= "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v panel-start)" = button ] && [ "$(v dialog-start)" = button ] && pass "set up: the button inside the panel has the focus" || fail "setup: $(v panel-start) $(v dialog-start)"
[ "$(v panel-destroyed)" = main ] && pass "panel destroyed: its parent window has the keyboard" || fail "panel destroyed: focus on $(v panel-destroyed)"
[ "$(v dialog-destroyed)" = main ] && pass "AnyDesk-shaped dialog and backdrop destroyed: the main window has the keyboard" || fail "dialog destroyed: focus on $(v dialog-destroyed)"
[ "$(v active)" = main ] && pass "the main window stays active" || fail "active: $(v active)"
[ "$(v child-destroyed)" = main ] && pass "a focused child destroyed: its parent has the keyboard (as before)" || fail "child destroyed: focus on $(v child-destroyed)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
