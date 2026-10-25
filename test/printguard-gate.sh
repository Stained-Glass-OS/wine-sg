#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# PrintWindow of a window whose process denies PROCESS_DUP_HANDLE (6101). PW_RENDERFULLCONTENT across
# processes duplicated a section into the window's process; KeePassXC's process DACL refuses that (it keeps
# others out of its memory), PrintWindow failed, and the taskbar's hover thumbnail fell back to the screen:
# a picture of whatever covered KeePassXC there (Slack). Now the window's process takes the section from
# the requester instead. The host process protects itself so and shows a green window, half covered by a
# gray one; the print process asks for the green one's picture.
#   WINE=/opt/wine-sg/bin/wine test/printguard-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutant: SG_MUTANT_PRINT_NO_PULL (win32u dce.c).
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
T=$(mktemp -d /var/tmp/sg-printguard.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/probe.exe" "$HERE/printguard-probe.c" -lgdi32 -ladvapi32 2>"$T/cc.log" ||
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
D="$WINEPREFIX/drive_c/pg"; mkdir -p "$D"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' host 'C:\pg' >/dev/null 2>&1 &
i=0; while [ ! -e "$D/host.ready" ] && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' print 'C:\pg' >/dev/null 2>&1 &
i=0; while ! grep -q '^right=' "$D/print.out" 2>/dev/null && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
out=$(tr -d '\r' < "$D/print.out" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v dup)" = refused ] || { echo "SKIP: the host's process DACL is not enforced here (dup=$(v dup))"; exit 77; }
[ "$(v ok)" = 1 ] && pass "PrintWindow of the guarded process's window succeeds" || fail "PrintWindow failed (ok=$(v ok))"
[ "$(v left)" = 00c800 ] && [ "$(v right)" = 00c800 ] && pass "the picture is the window's own green, the covered half too" \
    || fail "picture: left $(v left), right $(v right) (not the window's green 00c800)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
