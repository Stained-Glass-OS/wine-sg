#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell keeps running while framed Linux windows pass the focus around and ask for states
# (wine-sg 7827). winex11's sg_embed took X's error lock under embed_mutex (a framed window's focus
# change, its GTK shadow changing) while the state thread, hearing a program's _NET_WM_STATE request,
# held the error lock and wanted embed_mutex: explorer deadlocked. The shell was gone, no Windows
# program could start, and a full-screen frame stayed over everything (sdl-freerdp3 /multimon, its
# two full-screen windows passing the focus back and forth while it asked for states).
# A stand-in (embedlock-app.c) with two windows framed by the taskbar passes the X focus between
# them about a thousand times a second while each asks for maximized on and off, for 15 seconds;
# then:
#   - the shell still answers (a probe reads the taskbar's frames within 20 seconds), the program's
#     frames gone with it;
#   - a new Windows program still starts.
# Then the same with window A full screen and the program ending in the middle of the storm, its
# windows still up (a full-screen game exiting -- David's Enshrouded under Proton, 11-15 on 910: its
# frame stayed full screen over everything, the shell hung with the pointer grabbed, no Windows
# program started): no frame and no full-screen window of the shell's left, the pointer free.
#   WINE=/opt/wine-sg/bin/wine test/embedlock-gate.sh   (mutant SG_MUTANT_EMBED_LOCK_ORDER, winex11.drv sg_embed.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER WAYLAND_DISPLAY
for t in Xvfb xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedlock.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/app" "$HERE/embedlock-app.c" -lX11 || { echo "FAIL  embedlock-app did not build"; exit 1; }
cc -O2 -o "$T/grabcheck" "$HERE/embedmaxgrab-grabcheck.c" -lX11 || { echo "FAIL  grab check did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"
for MODE in windows fs; do
    L="$T/app-$MODE.log"; : > "$L"
    "$T/app" 15 "$L" $([ $MODE = fs ] && echo fs) & AP=$!
    i=0; while [ "$(cat "$L" | grep -c '^[AB] ')" -lt 2 ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    A=$(awk '$1 == "A" {print $2}' "$L"); B=$(awk '$1 == "B" {print $2}' "$L")
    printf '%d shown - lockA\tlockA\n%d shown - lockB\tlockB\nEND\n' "$A" "$B" > "$T/list"
    sleep 5
    framed=$(timeout 20 "$WINE" probe.exe list SgLinuxWindow - 2>/dev/null | tr -d '\r' | grep -c visible)
    [ "$framed" = 2 ] && pass "$MODE: both windows framed" || echo "NOTE  $MODE: framed: $framed"
    echo framed >> "$L"
    wait "$AP" 2>/dev/null; AP=
    printf 'END\n' > "$T/list"
    echo "      $MODE: $(grep '^done' "$L")"
    sleep 3
    timeout 20 "$WINE" probe.exe list SgLinuxWindow - > "$T/frames" 2>/dev/null; rc=$?
    r=$(tr -d '\r' < "$T/frames" | grep -c visible)
    [ "$rc" = 0 ] && [ "$r" = 0 ] && pass "$MODE: the shell still answers, and the frames went with the program (rc $rc, $r shown)" ||
        fail "$MODE: the shell: probe rc $rc (124: no answer in 20 s), frames shown $r"
    if [ $MODE = fs ]; then
        # a full-screen game exiting (David's Enshrouded, 11-15): no full-screen frame left over everything
        big=$(xwininfo -root -tree | awk '$0 ~ /explorer.exe/ && $0 ~ /1024x700\+0\+0/ && $0 !~ /Wine Desktop/' | grep -c .)
        [ "$big" = 0 ] && pass "$MODE: no full-screen window of the shell's left" || fail "$MODE: $big full-screen X window(s) of the shell's left"
        g=$("$T/grabcheck" 2>/dev/null)
        [ "$g" = pointer=free ] && pass "$MODE: the pointer is free ($g)" || fail "$MODE: the pointer: $g"
    fi
    s=$(timeout 30 "$WINE" probe.exe exists Shell_TrayWnd "" 2>/dev/null | tr -d '\r')
    case "$s" in *exists=1*) pass "$MODE: a new Windows program still starts (and finds the taskbar)" ;; *) fail "$MODE: a new Windows program: '$s'" ;; esac
done
if [ $RC != 0 ] && [ -n "${EMBEDLOCK_GDB:-}" ]; then
    EP=$(pgrep -f "[e]xplorer.exe /desktop=shell,1024x700" | head -1)
    X11=$(grep -m1 winex11.so /proc/$EP/maps | awk '{print $1}' | cut -d- -f1)
    echo "explorer $EP winex11 base $X11; embed_mutex +0x$(nm "$(dirname "$WINE")/dlls/winex11.drv/winex11.so" | awk '/ embed_mutex$/ {print $1}'), error_mutex +0x$(nm "$(dirname "$WINE")/dlls/winex11.drv/winex11.so" | awk '/ error_mutex$/ {print $1}')"
    for t in /proc/$EP/task/*; do echo "  $(basename $t) $(cat $t/wchan) $(cut -d' ' -f1-2 $t/syscall)"; done
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
