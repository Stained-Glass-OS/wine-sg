#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The MCIAVI configure dialog (patches/sg/6513; MCI_CONFIGURE was a stub, DRV_CONFIGURE a sample
# message box): Window/Full screen, Zoom by 2, Skip frames, kept per user and used by the next
# open and play (test/mciconfig-probe.c), under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/mciconfig-gate.sh
# Mutant: SG_MUTANT_MCIAVI_OPTIONS_NOT_SAVED (mciavi32 mciavi.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mciconfig.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/mciconfig-probe.c" -lavifil32 -lwinmm -ladvapi32 -lole32 -luuid -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check open               "the probe's AVI opens"
check dest_plain         "at its own size"
check configure_test     "configure test succeeds without a dialog"
check configure          "configure shows the dialog and returns"
check dialog             "the dialog is there"
check dialog_defaults    "showing the defaults (window, no zoom, skip frames)"
check saved              "OK keeps Full screen and Zoom by 2 for the user"
check dest_zoomed        "the next open is twice the size"
check plays_full_screen  "a plain play is full screen"
check dialog_shows_saved "the dialog shows the kept options"
check cancel_keeps       "Defaults then Cancel changes nothing"
check done         "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
