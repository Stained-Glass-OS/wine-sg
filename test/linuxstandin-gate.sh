#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux window's stand-in (the hidden SgLinuxWindow the taskbar keeps for
# it) answers as the window would (patches/sg/0758): WM_CLOSE -- Task
# Manager's End task -- asks the Linux window to close (XCLOSE), SC_RESTORE
# brings it forward (XACTIVATE). With DefWindowProc a close only destroyed the
# stand-in. A stand-in sg-lockctl (SG_LOCKCTL) lists one Linux window and
# records what it is asked.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxstandin-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb xdotool cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxstandin.XXXXXX); XP=
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
cd "$WINEPREFIX/drive_c"
: > "$T/commands"
"$WINE" vdesk-probe.exe wmclose SgLinuxWindow "Linux Terminal" > "$T/close.out" 2>&1
sleep 2
"$WINE" vdesk-probe.exe restore SgLinuxWindow "Linux Terminal" > "$T/restore.out" 2>&1
sleep 2
"$WINE" vdesk-probe.exe endtask SgLinuxWindow "Linux Terminal" > "$T/endtask.out" 2>&1
sleep 2
cmd=$(tr '\n' '|' < "$T/commands"); RC=0
echo "      asked: $cmd"
case "$cmd" in *"XCLOSE 4242"*) echo "PASS  WM_CLOSE to the stand-in (End task) asks the Linux window to close" ;;
    *) echo "FAIL  no XCLOSE: $(cat "$T/close.out")"; RC=1 ;; esac
case "$cmd" in *"XACTIVATE 4242"*) echo "PASS  SC_RESTORE brings it forward" ;; *) echo "FAIL  no XACTIVATE: $(cat "$T/restore.out")"; RC=1 ;; esac
# Task Manager's End task when it did not close (a hung one, 0772)
case "$cmd" in *"XKILL 4242"*) echo "PASS  End task on one that did not close ends it (XKILL)" ;; *) echo "FAIL  no XKILL: $(cat "$T/endtask.out")"; RC=1 ;; esac
"$WINE" vdesk-probe.exe exists SgLinuxWindow | grep -q 'exists=1' && echo "PASS  and the stand-in stays (until the window goes from the list)" \
    || { echo "FAIL  the stand-in was destroyed"; RC=1; }
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
