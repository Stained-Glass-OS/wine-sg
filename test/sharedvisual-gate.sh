#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A dcomp visual shared with another process (wine-sg 7757): the desktop device's undocumented
# partner interface {d14b6158-c3fa-4bce-9c1f-b61d8665eab0}, which the WebView2 runtime asks for when
# Office composes it into a task pane without a window of its own (msAbydosForWindowlessWV2): on
# E_NOINTERFACE the Copilot pane said "This feature could not be started". The host makes a shared
# visual and a handle to it; the browser opens a target on the handle and shows its own window in
# it (CreateSurfaceFromHwnd). test/sharedvisual-probe.c, host and child, under Xvfb, checks:
#   the partner is the desktop device (not the version 1 device), a handle only for a visual;
#   the child's window goes into the host's window, visible, where its process put it on screen;
#   it follows the host window when that moves, and its container goes with the target.
#   WINE=/opt/wine-sg/bin/wine test/sharedvisual-gate.sh
# Mutants (dlls/dcomp/device.c): SG_MUTANT_DC_NO_PARTNER, SG_MUTANT_DC_SHARED_NO_EMBED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sharedvisual.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:391
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/sharedvisual-probe.c" \
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
expect V1Partner 0x80004002 "the version 1 device has no partner interface"
expect Partner 0x00000000 "the desktop device answers the partner interface"
expect PartnerSame 1 "and is it"
expect CreateShared 0x00000000 "CreateSharedVisual makes a visual"
expect HandleOfNonVisual 0x80070057 "a handle for something not a visual: E_INVALIDARG"
expect Handle 0x00000000 "a handle for the shared visual"
expect OpenTarget 0x00000000 "another process opens a target on the handle"
expect SurfaceFromHwnd 0x00000000 "and shows a window of its own in it"
expect PopupChild 1 "the window goes into the host's window"
expect PopupVisible 1 "shown"
expect PopupRect "110,140 300x200" "where its process put it on the screen"
host=$(printf '%s\n' "$out" | sed -n 's/^Host=//p')
expect ContainerParent "$host" "inside the host's window (its container's parent)"
expect ContainerOrigin "0,0" "the host window moved: the container follows (origin stays the screen's)"
expect ContainerGone 1 "the container goes with the target"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
