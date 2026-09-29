#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Programs pinned to the taskbar (patches/sg/0485). David: "you can pin one of
# the open icons to the panel and it remains there". A running program's
# button menu offers "Pin to taskbar"; the pin is a shortcut where Windows
# keeps them (%APPDATA%\Microsoft\Internet Explorer\Quick Launch\User
# Pinned\TaskBar), in the order HKCU\Software\Stained Glass\Taskbar PinOrder
# lists. A pinned program not running keeps an icon button, which starts it;
# running, its windows' buttons stand in the pin's place, first after Start.
# The pin's menu unpins it.
#
#   WINE=/opt/wine-sg/bin/wine test/taskpins-gate.sh
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

T=$(mktemp -d /var/tmp/sg-taskpins.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
PINS="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
S() { echo "== \$1" >> "$T/log.out"; "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
apply() { "$WINE" taskbar-probe.exe apply >/dev/null 2>&1; sleep 2; }
mid() { echo "\$1" | awk -F, '{ printf "%d %d", (\$1 + \$3) / 2, (\$2 + \$4) / 2 }'; }
first() { "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n "s/^\$1=//p" | head -1; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" notepad >/dev/null 2>&1 &
i=0; while [ \$i -lt 60 ] && ! "$WINE" taskbar-probe.exe state 2>/dev/null | grep -q "^windows=1"; do sleep 1; i=\$((i + 1)); done
sleep 1
S running
# the running notepad's button: its menu, Pin to taskbar
xdotool mousemove \$(mid "\$(first button)") click 3; sleep 1.5; xdotool key k; sleep 2
ls "$PINS" > "$T/pins.out" 2>&1
S pinned
"$WINE" reg query 'HKCU\Software\Stained Glass\Taskbar' /v PinOrder 2>/dev/null | tr -d '\r' > "$T/order.out"
# notepad closes: its pin stays
xdotool key ctrl+q 2>/dev/null; "$WINE" taskkill /im notepad.exe >/dev/null 2>&1; sleep 3
S closed
# a second program, pinned by hand where Windows keeps pins, first in the order
"$WINE" taskbar-probe.exe window Other 300 200 >/dev/null 2>&1 &
sleep 2
S other
P=\$(first pin)
xdotool mousemove \$(mid "\$P") click 1; sleep 4
S started
# the button now in the pin's place is notepad's: its menu, Unpin from taskbar
set -- \$(mid "\$P")
xdotool mousemove \$1 \$2 click 3; sleep 1.5; import -window root "$T/menu.png"
# the menu stands on the pointer: its name, Unpin from taskbar, a line, Close window
xdotool mousemove \$((\$1 + 50)) \$((\$2 - 57)) click 1; sleep 2
ls "$PINS" > "$T/unpinned.out" 2>&1
"$WINE" taskkill /im notepad.exe >/dev/null 2>&1; sleep 3
S gone
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

v() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { sub(k "=", ""); print; exit }' "$T/log.out"; }
count() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { n++ } END { print n + 0 }' "$T/log.out"; }
left() { echo "$1" | cut -d, -f1; }

grep -qi 'notepad.*\.lnk' "$T/pins.out" && pass "Pin to taskbar: a shortcut in User Pinned\\TaskBar ($(cat "$T/pins.out"))" || fail "no pin shortcut: $(cat "$T/pins.out")"
grep -qi 'PinOrder.*notepad' "$T/order.out" && pass "and its place in PinOrder" || fail "PinOrder: $(cat "$T/order.out")"
[ "$(count pinned pin)" = 0 ] && [ "$(count pinned button)" = 1 ] && pass "running, the pinned program has its window's button only" || fail "pinned: pins $(count pinned pin) buttons $(count pinned button)"
[ "$(count closed pin)" = 1 ] && [ "$(count closed button)" = 0 ] && pass "closed, its pin stays on the bar" || fail "closed: pins $(count closed pin) buttons $(count closed button)"
[ "$(left "$(v other pin)")" -lt "$(left "$(v other button)")" ] 2>/dev/null && pass "pins come first after Start, before other programs' buttons" || fail "order: pin $(v other pin) button $(v other button)"
minleft() { awk -v s="== $1" '$0 == s { on = 1; next } /^== / { on = 0 } on && /^button=/ { sub(/^button=/, ""); split($0, r, ","); if (m == "" || r[1] < m) m = r[1] } END { print m }' "$T/log.out"; }
[ "$(count started pin)" = 0 ] && [ "$(count started button)" = 2 ] && [ "$(minleft started)" = "$(left "$(v other pin)")" ] \
    && pass "the pin starts the program; its button takes the pin's place" || fail "started: pins $(count started pin) buttons $(count started button) first at $(minleft started)"
grep -qi 'notepad' "$T/unpinned.out" && fail "Unpin left the shortcut: $(cat "$T/unpinned.out")" || pass "Unpin from taskbar removes the shortcut"
[ "$(count gone pin)" = 0 ] && pass "unpinned and closed: nothing of it on the bar" || fail "gone: pins $(count gone pin)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
