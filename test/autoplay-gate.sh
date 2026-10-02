#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A USB drive that arrives gets AutoPlay's notification, and selecting it
# opens the drive in File Explorer (patches/sg/0748; David 2026-10-01:
# plugging a USB drive did nothing). Here a removable drive (R:) is defined
# through mount manager while the shell runs: a balloon must come up, and a
# click on it must open a window on R:. The tray tells a balloon's owner of
# the click (NIN_BALLOONUSERCLICK), as Windows does.
#
#   WINE=/opt/wine-sg/bin/wine test/autoplay-gate.sh
# Mutants: -DSG_MUTANT_NO_AUTOPLAY (no balloon), -DSG_MUTANT_NO_BALLOON_CLICK
# (the click does nothing).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-autoplay.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${SG_KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/drivewatch-probe.exe" "$HERE/drivewatch-probe.c" || { fail "probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/autoplay-probe.exe" "$HERE/autoplay-probe.c" -lcomctl32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX" "$T/stick"
echo "a file on the stick" > "$T/stick/readme.txt"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/drivewatch-probe.exe" "$T/autoplay-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
sleep 8
(cd "$WINEPREFIX/drive_c" && timeout 90 "$WINE" autoplay-probe.exe R 25 > "$T/probe.out" 2>/dev/null &)
sleep 1
(cd "$WINEPREFIX/drive_c" && "$WINE" drivewatch-probe.exe define R "$T/stick" > "$T/define.out" 2>/dev/null)
i=0; while ! grep -q '^opened=' "$T/probe.out" 2>/dev/null && ! grep -q '^balloon=0' "$T/probe.out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
echo "      $(cat "$T/define.out" 2>/dev/null); $(tr -d '\r' < "$T/probe.out" | tr '\n' ' ')"
grep -q '^balloon=1' "$T/probe.out" && pass "a removable drive arriving brings up a notification" || fail "no notification for R:"
opened=$(tr -d '\r' < "$T/probe.out" | sed -n 's/^opened=//p')
[ -n "$opened" ] && pass "selecting it opens the drive ($opened)" || fail "selecting it opened nothing"
exit $RC
