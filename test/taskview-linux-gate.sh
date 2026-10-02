#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The session's Linux programs' windows in Task View (patches/sg/0585).
# A Linux terminal has a taskbar button (0496) but was missing from Task
# View. Now it is a card there (its icon and title, on every desktop); a click
# brings it forward (XACTIVATE); so does choosing it in Alt+Tab's switcher (0760). A stand-in sg-lockctl (SG_LOCKCTL) lists one
# Linux window and records what it is asked; Task View is opened with Win+Tab
# and its only card -- centred below the desktops strip -- is clicked.
#
#   WINE=/opt/wine-sg/bin/wine test/taskview-linux-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb xdotool cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tvlinux.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf '4242 shown - XTerm\tLinux Terminal\nEND\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run" SG_TASKVIEW_ANIM_MS=0
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 5    # the taskbar reads the list
: > "$T/commands"
cd "$WINEPREFIX/drive_c"
"$WINE" vdesk-probe.exe hotkey taskview >/dev/null 2>&1 &
sleep 3
xdotool mousemove 512 428 click 1
sleep 2
cmd=$(tr '\n' '|' < "$T/commands"); RC=0
echo "      asked: $cmd"
case "$cmd" in
*"XACTIVATE 4242"*) echo "PASS  the Linux window is a card in Task View, and a click brings it forward" ;;
*) echo "FAIL  no Linux window card in Task View (asked: $cmd)"; RC=1 ;;
esac
# Alt+Tab's switcher lists it too (0760): chosen, it comes forward
: > "$T/commands"
"$WINE" vdesk-probe.exe alttab > "$T/alttab.out" 2>&1
sleep 2
cmd=$(tr '\n' '|' < "$T/commands")
case "$cmd" in
*"XACTIVATE 4242"*) echo "PASS  Alt+Tab lists the Linux window, and choosing it brings it forward" ;;
*) echo "FAIL  Alt+Tab did not bring the Linux window forward (asked: $cmd; $(cat "$T/alttab.out"))"; RC=1 ;;
esac
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
