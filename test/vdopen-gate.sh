#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program opened on desktop 2 opens on desktop 2, and the shell stays there
# (wine-sg 7803; David 2026-11-15: "If you're on desktop 2, when you open up
# an application, it opens up on desktop 1 and switches you over there").
# On desktop 2 (of 2), with Notepad already open on desktop 1:
#   - Notepad again (a second instance)
#   - File Explorer (a window of a separate explorer.exe process)
#   - a program started through start.exe, as a launcher starts it
#   - a framed Linux program (a stand-in X window, listed as sg-compositor
#     lists it)
# each opens on desktop 2 and desktop 2 stays the current one. First, a
# launcher (a tool window, as Start) goes away on desktop 2: the cause --
# Wine gave the foreground to the next window in its Z order, cloaked on
# desktop 1, and the shell went there before the program opened (win32u
# can_activate_window now passes over cloaked windows).
#   WINE=/opt/wine-sg/bin/wine test/vdopen-gate.sh
# Mutant: SG_MUTANT_ACTIVATE_CLOAKED (dlls/win32u/window.c) fails the launcher check.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vdopen.XXXXXX); XP=; AP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$AP" ] && kill "$AP" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/app" "$HERE/embedconf-app.c" -lX11 || { echo "FAIL  embedconf-app did not build"; exit 1; }
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { echo "FAIL  probe did not build"; exit 1; }
if [ -n "${VDOPEN_DISPLAY:-}" ]; then
    Xvfb ":$VDOPEN_DISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$VDOPEN_DISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops' /v VirtualDesktopIDs \
    /t REG_BINARY /d 5347564431000000a1b2c3d4e5f60718 /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${VD_DEBUG:--all}" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"
P() { "$WINE" vdesk-probe.exe "$@" 2>/dev/null | tr -d '\r'; }

# Notepad on desktop 1; then desktop 2, and go there
"$WINE" notepad >/dev/null 2>&1 &
sleep 4
P new; sleep 2
q=$(P query)
[ "$q" = "desktops=2 current=1" ] || { fail "desktop 2 was not made current ($q)"; exit 1; }

# Start (a passing tool window, the program's launcher) on desktop 2 goes away
# as it starts a program: Wine gave the foreground to the next window in its
# Z order -- Notepad, cloaked on desktop 1 -- and the shell went there; the
# program, opening a moment later, opened there too
"$WINE" vdesk-probe.exe toolwindow Launcher 300 200 >/dev/null 2>&1 &
sleep 2.5
P closewin Launcher >/dev/null; sleep 2.5
q=$(P query); f=$(P foreground)
[ "$q" = "desktops=2 current=1" ] && pass "a launcher (tool window) going away on desktop 2: desktop 2 stays ($q, $f)" \
    || fail "a launcher going away on desktop 2: $q, $f (want current=1: the foreground fell to a window on desktop 1)"
P switch 1; sleep 1.5

opened() {   # NAME CLASS: its window on desktop 2 (index 1) and desktop 2 current
    i=0; while [ "$(P cstate "$2")" = "no window" ] && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
    sleep 2.5
    s=$(P cstate "$2"); q=$(P query)
    case "$s" in *"cloaked=0 desktop=1") ok=1 ;; *) ok=0 ;; esac
    [ $ok = 1 ] && [ "$q" = "desktops=2 current=1" ] && pass "$1 opened on desktop 2 opens there, and desktop 2 stays ($s; $q)" \
        || fail "$1 opened on desktop 2: its window $s; $q (want desktop=1, current=1)"
}

"$WINE" explorer 'C:\' >/dev/null 2>&1 &
opened "File Explorer" ExplorerWClass
"$WINE" start /unix "$T/vdesk-probe.exe" window Started 300 200 >/dev/null 2>&1 &
opened "a program started through start.exe" SgVdeskProbe
L="$T/app.log"; : > "$L"
"$T/app" "$L" & AP=$!
i=0; while ! grep -q '^main ' "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(awk '$1 == "main" {print $2; exit}' "$L")
printf '%d shown - DockTest\tDock Test\nEND\n' "$XID" > "$T/list"
opened "a framed Linux program" SgLinuxWindow
# Notepad again: a second instance's window
"$WINE" notepad >/dev/null 2>&1 &
sleep 4
n=$(P cstate Notepad); q=$(P query)
[ "$q" = "desktops=2 current=1" ] && pass "Notepad opened again on desktop 2: desktop 2 stays ($q)" \
    || fail "Notepad opened again on desktop 2: $q (want current=1); first Notepad window $n"
[ $RC = 0 ] && echo "vdopen: all passed"
exit $RC
