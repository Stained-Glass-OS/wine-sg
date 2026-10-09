#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A USB drive that arrives gets AutoPlay's notification, and selecting it
# opens the drive in File Explorer (patches/sg/0748; David 2026-10-01:
# plugging a USB drive did nothing). Here a removable drive (R:) is defined
# through mount manager while the shell runs: a balloon must come up, and a
# click on it must open a window on R:. The tray tells a balloon's owner of
# the click (NIN_BALLOONUSERCLICK), as Windows does.
# 1514: selecting it asks, as Windows 10 does (Open folder to view files /
# Take no action), and the answer is kept where Windows keeps it
# (Explorer\AutoplayHandlers\UserChosenExecuteHandlers StorageOnArrival):
# with Open folder kept the next drive opens at once, no notification; Take
# no action, or AutoPlay off (AutoplayHandlers DisableAutoplay = 1): nothing.
#
#   WINE=/opt/wine-sg/bin/wine test/autoplay-gate.sh
# Mutants: -DSG_MUTANT_NO_AUTOPLAY (no balloon), -DSG_MUTANT_NO_BALLOON_CLICK
# (the click does nothing), -DSG_MUTANT_AUTOPLAY_NOT_KEPT (the answer not
# kept), -DSG_MUTANT_AUTOPLAY_IGNORES_CHOICE (a kept answer or AutoPlay off
# ignored).
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
grep -q '^asked=1' "$T/probe.out" && pass "selecting it asks what to do (AutoPlay)" || fail "no AutoPlay question"
opened=$(tr -d '\r' < "$T/probe.out" | sed -n 's/^opened=//p')
[ -n "$opened" ] && pass "Open folder to view files opens the drive ($opened)" || fail "selecting it opened nothing"
AP='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers'
chosen() { "$WINE" reg query "$AP\\UserChosenExecuteHandlers" /v StorageOnArrival 2>/dev/null | tr -d '\r' | awk '/StorageOnArrival/ { print $3 }'; }
[ "$(chosen)" = MSOpenFolder ] && pass "...and the answer is kept (StorageOnArrival = MSOpenFolder)" || fail "kept: '$(chosen)'"

# the next drives, with an answer kept or AutoPlay off
arrive() {   # LETTER MODE: the drive arrives; the probe's lines
    (cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" autoplay-probe.exe "$1" "${3:-15}" "$2" > "$T/probe-$1.out" 2>/dev/null &)
    sleep 1
    (cd "$WINEPREFIX/drive_c" && "$WINE" drivewatch-probe.exe define "$1" "$T/stick" >/dev/null 2>&1)
    i=0; while ! grep -q '^opened=' "$T/probe-$1.out" 2>/dev/null && [ $i -lt 80 ]; do sleep 0.5; i=$((i + 1)); done
    tr -d '\r' < "$T/probe-$1.out" | tr '\n' ' '
}
out=$(arrive S direct)
case "$out" in "balloon=0 opened="*S:*) pass "Open folder kept: the next drive opens at once, no notification ($out)" ;;
               *) fail "with Open folder kept: $out" ;; esac
"$WINE" reg add "$AP\\UserChosenExecuteHandlers" /v StorageOnArrival /d MSTakeNoAction /f >/dev/null 2>&1
out=$(arrive T nothing 8)
[ "$out" = "balloon=0 opened= " ] && pass "Take no action kept: nothing" || fail "with Take no action kept: $out"
"$WINE" reg delete "$AP\\UserChosenExecuteHandlers" /v StorageOnArrival /f >/dev/null 2>&1
"$WINE" reg add "$AP" /v DisableAutoplay /t REG_DWORD /d 1 /f >/dev/null 2>&1
out=$(arrive U nothing 8)
[ "$out" = "balloon=0 opened= " ] && pass "AutoPlay off (DisableAutoplay): nothing" || fail "with AutoPlay off: $out"
exit $RC
