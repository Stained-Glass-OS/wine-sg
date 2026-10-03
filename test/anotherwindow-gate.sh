#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Another window of a running program from its taskbar button (patches/sg/0773):
# Shift+click or a middle click starts the program again, as Windows' taskbar
# does (David 2026-10-02: "you need to be able to open more than one" File
# Explorer); a plain click only brings its window forward.
#
# Notepad runs in a shell session; its button (taskbar-probe's state) is
# clicked: plainly (still one window), with Shift (two), with the middle
# button (three). Mutant SG_MUTANT_NO_ANOTHER_WINDOW fails it.
#
#   WINE=/opt/wine-sg/bin/wine test/anotherwindow-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run xdotool; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-anotherwindow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
count() { "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | grep -c '^button='; }
"$WINE" explorer /desktop=shell,1024x700 > /dev/null 2>&1 &
sleep 6
"$WINE" notepad > /dev/null 2>&1 &
i=0; while [ \$i -lt 40 ] && [ "\$(count)" -lt 1 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 1
set -- \$("$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n 's/^button=//p' | head -1 | tr ',' ' ')
x=\$(( (\$1 + \$3) / 2 )); y=\$(( (\$2 + \$4) / 2 ))
echo "button \$x \$y" > "$T/where"
xdotool mousemove \$x \$y click 1; sleep 3; count > "$T/plain"
xdotool mousemove \$x \$y keydown shift click 1 keyup shift; sleep 4; count > "$T/shift"
xdotool mousemove \$x \$y click 2; sleep 4; count > "$T/middle"
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s "-screen 0 1024x700x24" "$T/session.sh" > "$T/session.out" 2>&1
n() { cat "$T/$1" 2>/dev/null || echo '?'; }
echo "      $(cat "$T/where" 2>/dev/null): plain $(n plain), shift $(n shift), middle $(n middle)"
[ "$(n plain)" = 1 ] && pass "a plain click brings the window forward (still one)" || fail "plain click: $(n plain) windows"
[ "$(n shift)" = 2 ] && pass "Shift+click starts another window of it" || fail "Shift+click: $(n shift) windows"
[ "$(n middle)" = 3 ] && pass "a middle click starts another too" || fail "middle click: $(n middle) windows"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
