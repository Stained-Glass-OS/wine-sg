#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop's colour survives a light/dark switch (6090). The visual style's colour schemes carry every
# system colour, the desktop's (COLOR_BACKGROUND, 37 111 149) too, and switching scheme with the app mode
# (Settings > Colours > Dark; RefreshImmersiveColorPolicyState) wrote all of them, so a solid desktop colour
# the person chose went back to the scheme's at every switch. Windows leaves it alone: now the switch changes
# the other colours only. The probe sets the desktop magenta as Settings does, then goes dark and light again.
#   WINE=/opt/wine-sg/bin/wine test/schemedesktop-gate.sh   (mutant SG_MUTANT_SCHEME_SETS_DESKTOP)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-schemedesktop.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/schemedesktop-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect before 255,0,255 "the desktop colour is set (magenta)"
expect dark_window_changed 1 "going dark changes the scheme's colours"
expect dark 255,0,255 "but not the desktop's"
expect dark_reg 255,0,255 "nor the saved one"
expect light_window_back 1 "going light again brings the light colours back"
expect light 255,0,255 "and the desktop is still magenta"
expect light_reg 255,0,255 "saved too"
expect done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
