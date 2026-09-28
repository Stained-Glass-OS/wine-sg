#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's taskbar button has Windows 10's menu (patches/sg/0473): the
# program's name (another window of it), Report a problem (sg-bugreport,
# when it is installed) and Close window; Shift+right-click keeps the
# window's own system menu. Here: the menu opens on Notepad's button, Close
# window closes it, the name starts a second Notepad, and Shift+right-click
# still gives a menu.
#
#   WINE=/opt/wine-sg/bin/wine test/btnmenu-gate.sh
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

T=$(mktemp -d /var/tmp/sg-btnmenu.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/btnmenu-probe.exe" "$HERE/btnmenu-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/btnmenu-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" btnmenu-probe.exe "\$1" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
start_notepad() {
    "$WINE" notepad >/dev/null 2>&1 &
    i=0; while [ \$i -lt 60 ] && ! "$WINE" btnmenu-probe.exe button 2>/dev/null | grep -q 'button [0-9]'; do sleep 1; i=\$((i + 1)); done
    sleep 1
}
button() { "$WINE" btnmenu-probe.exe button 2>/dev/null | tr -d '\r' | awk '{print \$2, \$3}'; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
start_notepad
P count
B=\$(button)
xdotool mousemove \$B click 3; sleep 2; P menu
xdotool key l; sleep 3; P count
"$WINE" btnmenu-probe.exe count 2>/dev/null | grep -q 'notepads 0' || { "$WINE" taskkill /f /im notepad.exe >/dev/null 2>&1; sleep 2; }
start_notepad
P count
B=\$(button)
xdotool mousemove \$B click 3; sleep 2; xdotool key Down Return; sleep 4; P count
B=\$(button)
xdotool mousemove \$B keydown shift click 3 keyup shift; sleep 2; P menu; xdotool key Escape
"$WINESERVER" -k
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null
n() { sed -n "$1p" "$T/log.out" 2>/dev/null; }
[ "$(n 1)" = "notepads 1" ] || fail "Notepad never came up: $(n 1)"
[ "$(n 2)" = "menu shown" ] && pass "right-clicking a program's button opens its menu" || fail "no menu on the button: $(n 2)"
[ "$(n 3)" = "notepads 0" ] && pass "Close window closes it" || fail "Close window: $(n 3)"
[ "$(n 4)" = "notepads 1" ] || fail "Notepad did not start again: $(n 4)"
[ "$(n 5)" = "notepads 2" ] && pass "the program's name starts another window of it" || fail "the name: $(n 5)"
[ "$(n 6)" = "menu shown" ] && pass "Shift+right-click still opens the window's menu" || fail "Shift+right-click: $(n 6)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
