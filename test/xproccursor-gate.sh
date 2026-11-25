#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Another process's cursor can be drawn and read (patches/sg/9107; under Wine a cursor's bitmaps
# lived only in the process that made it, so screen recorders drew no pointer from GetCursorInfo's
# handle): DrawIconEx, GetIconInfo and SetCursor in a child on the parent's 32-bit and monochrome
# cursors; icons stay private; a destroyed cursor is gone (test/xproccursor-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/xproccursor-gate.sh
# Mutants: SG_MUTANT_CURSOR_NO_PUBLISH (win32u cursoricon.c)
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
T=$(mktemp -d /var/tmp/sg-xproccursor.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/xproccursor-probe.c" -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check own_draw           "the parent draws its own cursor"
check alpha_draw         "a child draws the parent's 32-bit cursor"
check alpha_info         "GetIconInfo gives its hotspot and bitmaps"
check alpha_bitmap       "the color bitmap is the cursor's size"
check mono_draw          "the parent's monochrome cursor: transparent where it should be"
check mono_black         "and black where it should be"
check mono_info          "GetIconInfo: no color bitmap, the hotspot"
check mono_mask          "the AND-over-XOR mask"
check set_cursor         "SetCursor returns the other process's cursor"
check icon_private       "icons stay the process's own"
check bogus              "a handle that is nothing fails"
check destroyed          "a cursor its process destroyed no longer draws"
check still_there        "the other cursor still does"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
