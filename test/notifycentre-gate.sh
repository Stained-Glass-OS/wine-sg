#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The notification centre's Windows side (patches/sg/0815): a balloon a
# program shows from its notification-area icon (Shell_NotifyIcon NIF_INFO)
# is kept in the user's notification history, as Windows 10 keeps it in its
# action centre (toasts: test/toast-gate.sh), and Win+A opens the centre
# (sg-shell's sg-notify, run through App Paths). Under Xvfb, the shell's
# taskbar; a stand-in for sg-notify.exe.
#
#   WINE=/opt/wine-sg/bin/wine test/notifycentre-gate.sh   (mutant SG_MUTANT_NO_NOTIFICATION_HISTORY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-notifycentre.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/notifycentre-probe.c" || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
# the stand-in, where Win+A finds sg-notify.exe
cp "$T/probe.exe" "$WINEPREFIX/drive_c/sg-notify.exe"
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\sg-notify.exe' /ve /d 'C:\sg-notify.exe' /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 &
sleep 8

out=$("$WINE" 'C:\probe.exe' balloon 2>/dev/null | tr -d '\r')
hist=$("$WINE" reg query 'HKCU\Software\Stained Glass\Notifications\History' /s 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q 'balloon=1' || fail "the balloon was not shown: $out"
printf '%s\n' "$hist" | grep -q 'Title.*REG_SZ.*Backup finished' && printf '%s\n' "$hist" | grep -q 'Body.*REG_SZ.*Your files were copied' \
    && pass "a tray icon's balloon is kept in the notification history" || fail "balloon history: $(printf '%s\n' "$hist" | head -8 | tr '\n' '|')"
printf '%s\n' "$hist" | grep -q 'App.*REG_SZ.*Balloon Probe' && pass "under its program's name (the icon's tooltip)" \
    || fail "balloon's program: $(printf '%s\n' "$hist" | grep App)"

# Win+A: the centre (the stand-in records how it was asked)
rm -f "$WINEPREFIX/drive_c/notify-args.txt"
xdotool key super+a; sleep 3
[ "$(cat "$WINEPREFIX/drive_c/notify-args.txt" 2>/dev/null | tr -d ' ')" = "/toggle" ] \
    && pass "Win+A runs sg-notify.exe /toggle (the notification centre)" || fail "Win+A: '$(cat "$WINEPREFIX/drive_c/notify-args.txt" 2>/dev/null)'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
