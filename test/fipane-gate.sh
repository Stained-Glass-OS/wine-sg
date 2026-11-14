#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# FrameworkInputPane (wine-sg 7782): the documented COM face of the input pane (IFrameworkInputPane,
# shell32). Windows always has it; Avalonia creates it for every window and stopped with
# REGDB_E_CLASSNOTREG (ILSpy 11 died at its start). It is the window's Windows.UI.ViewManagement
# InputPane (the sg-osk keyboard): AdviseWithHWND registers a handler that is told when the keyboard
# shows (where, in screen pixels) and hides; Location says where it is; Unadvise ends it.
# test/fipane-probe.c plays the keyboard (an OSKMainClass window) under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/fipane-gate.sh
# Mutant (dlls/shell32/inputpane.c): SG_MUTANT_FIP_NO_PANE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fipane.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:395
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/fipane-probe.c" -lole32 -luuid -luser32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY="$DISP" "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
out=$(cd "$T" && timeout -s KILL 120 env DISPLAY="$DISP" "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect Create 0x00000000 "CoCreateInstance(CLSID_FrameworkInputPane) gives an IFrameworkInputPane"
expect Advise "0x00000000 1" "AdviseWithHWND registers a handler (a cookie)"
expect LocationNone "0x00000000 0,0,0,0" "Location while no keyboard shows: empty"
expect Showing 1 "the keyboard shows: the handler's Showing is called"
expect ShownRect "100,450,700,650" "with where it covers the screen"
expect LocationShown "0x00000000 100,450,700,650" "Location says the same"
expect Hiding 1 "the keyboard hides: Hiding is called"
expect Unadvise 0x00000000 "Unadvise"
expect UnadviseAgain 0x80070057 "an unknown cookie: E_INVALIDARG"
expect AfterUnadvise 0 "an unadvised handler is not called any more"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
