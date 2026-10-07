#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WM_SYSCOMMAND SC_MAXIMIZE to a hidden window that is already maximized
# (patches/sg/1300). Chromium's views (Widget::Init) create a browser window
# with WS_MAXIMIZE when it last ran maximized and then send it SC_MAXIMIZE
# while it is still hidden; Wine showed and activated it there, and Edge 154
# crashed in its WM_ACTIVATE handler (msedge.dll+0x2eeff68, a read at 0xb0:
# the window had no tab yet) on every start once its window had been
# maximized (David's Latitude 2026-10-06). The window must stay hidden and
# inactive; ShowWindow(SW_SHOWMAXIMIZED) later shows and activates it; a
# visible window still maximizes. Mutant: SG_MUTANT_HIDDEN_SC_MAXIMIZE.
#
#   WINE=/opt/wine-sg/bin/wine test/hiddenmax-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${HIDDENMAX_DPY:-211}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hiddenmax.XXXXXX)
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/hiddenmax-probe.c" -mwindows || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 1
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v hidden-visible)" = 0 ] && pass "SC_MAXIMIZE leaves a hidden maximized window hidden" || fail "hidden window shown: $(v hidden-visible)"
[ "$(v hidden-activations)" = 0 ] && [ "$(v hidden-active)" = 0 ] && pass "...and does not activate it (no WM_ACTIVATE)" \
    || fail "hidden window activated: $(v hidden-activations) WM_ACTIVATE, active $(v hidden-active)"
[ "$(v hidden-zoomed)" = 1 ] && pass "...and it is still maximized" || fail "not maximized: $(v hidden-zoomed)"
[ "$(v shown-visible)" = 1 ] && [ "$(v shown-active)" = 1 ] && [ "$(v shown-zoomed)" = 1 ] \
    && pass "ShowWindow(SW_SHOWMAXIMIZED) then shows it maximized and active" \
    || fail "shown: visible $(v shown-visible) active $(v shown-active) zoomed $(v shown-zoomed)"
[ "$(v visible-zoomed)" = 1 ] && pass "a visible window still maximizes on SC_MAXIMIZE" || fail "visible window not maximized"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
