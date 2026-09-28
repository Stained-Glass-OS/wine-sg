#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Right-clicking the taskbar opens its menu; right-clicking Start opens the
# quick link menu (patches/sg/0464).
#
# The bar's own menu, as Windows 10's: Search (hidden, icon, box), Show Task
# View button, Show the desktop, Task Manager, Lock the taskbar, Taskbar
# settings. A right-click on the bar did nothing. Here: the menu opens on
# empty bar space, Show the desktop minimizes Notepad, Lock the taskbar and
# Show Task View button write what Settings writes, and Start's right-click
# gives the Win+X menu.
#
#   WINE=/opt/wine-sg/bin/wine test/traymenu-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-traymenu.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/traymenu-probe.exe" "$HERE/traymenu-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/traymenu-probe.exe" "$WINEPREFIX/drive_c/"
# every program joins the shell's desktop, as in the session
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

ADV='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" traymenu-probe.exe "\$1" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
Q() { echo "\$1 \$("$WINE" reg query "$ADV" /v "\$1" 2>/dev/null | tr -d '\r' | awk '/REG_DWORD/ {print \$3}')" >> "$T/log.out"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" notepad >/dev/null 2>&1 &
i=0; while [ \$i -lt 60 ] && ! "$WINE" traymenu-probe.exe notepad 2>/dev/null | grep -q shown; do sleep 1; i=\$((i + 1)); done
P notepad
# empty bar space, left of the notification area
xdotool mousemove 640 680 click 3; sleep 2; P menu
xdotool key d; sleep 2; P notepad
xdotool mousemove 640 680 click 3; sleep 2; xdotool key o; sleep 2; Q TaskbarSizeMove
xdotool mousemove 640 680 click 3; sleep 2; xdotool key v; sleep 2; Q ShowTaskViewButton
# Start
xdotool mousemove 20 680 click 3; sleep 2; P menu; xdotool key Escape
"$WINESERVER" -k
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null
n() { sed -n "$1p" "$T/log.out" 2>/dev/null; }
[ "$(n 1)" = "notepad shown" ] || fail "Notepad never came up: $(n 1)"
[ "$(n 2)" = "menu shown" ] && pass "right-clicking the bar opens its menu" || fail "no menu on the bar: $(n 2)"
[ "$(n 3)" = "notepad minimized" ] && pass "Show the desktop minimizes the windows" || fail "Show the desktop: $(n 3)"
[ "$(n 4)" = "TaskbarSizeMove 0x1" ] && pass "Lock the taskbar unlocks it (TaskbarSizeMove, as Settings)" || fail "lock: $(n 4)"
[ "$(n 5)" = "ShowTaskViewButton 0x0" ] && pass "Show Task View button hides it (ShowTaskViewButton)" || fail "task view: $(n 5)"
[ "$(n 6)" = "menu shown" ] && pass "right-clicking Start opens the quick link menu" || fail "no menu on Start: $(n 6)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
