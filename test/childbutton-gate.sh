#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window put into another window has no taskbar button (6098). The taskbar gave a button to every
# visible unowned window that was created top-level, also after it became another window's child:
# comdlg32's file dialog creates its custom-controls pane top-level and puts it into the dialog, and
# Shotcut's Export dialog had a second Shotcut button with an icon and no title. As on Windows, only
# top-level windows have buttons. The probe shows two top-level windows and makes a third, created
# top-level, a visible child of the first, then lists the shell taskbar's buttons.
#   WINE=/opt/wine-sg/bin/wine test/childbutton-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutant: SG_MUTANT_CHILD_BUTTON (explorer systray.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-childbutton.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/childbutton-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' 'C:\probe.out' >/dev/null 2>&1 &
i=0; while ! grep -q done=1 "$WINEPREFIX/drive_c/probe.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/probe.out" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^done=1$' || { fail "the probe did not finish"; echo "RESULT: FAIL"; exit 1; }
printf '%s\n' "$out" | grep -q '^pane_child=1$' || fail "the pane did not become a visible child"
printf '%s\n' "$out" | grep -q '^button=main$' && printf '%s\n' "$out" | grep -q '^button=other$' \
    && pass "the two top-level windows have a button each" || fail "top-level windows' buttons missing"
printf '%s\n' "$out" | grep -q '^button=pane$' && fail "the window put into another has a taskbar button" \
    || pass "the window put into another window has no button"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
