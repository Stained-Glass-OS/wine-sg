#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Virtual desktops and Task View (patches/sg/0068), in a shell session.
#
# Two windows on desktop 1; Win+Ctrl+D makes desktop 2 and goes there (the
# windows are shell-cloaked: still visible to their programs, gone from the
# screen and the taskbar); a window opened there stays there; Win+Ctrl+Left
# goes back; Win+Ctrl+Shift+Right carries the active window along;
# Alt+Tab (on the X keyboard) switches between them;
# IVirtualDesktopManager agrees throughout and refuses to move another
# program's window; Win+Tab opens Task View and Escape closes it; the state
# is in HKCU\...\Explorer\VirtualDesktops; Win+Ctrl+F4 closes a desktop and
# its windows come back.
#
#   WINE=/opt/wine-sg/bin/wine test/vdesk-gate.sh
#   ARTIFACTS=DIR keeps screenshots and logs; EXPLORER_DEBUG=channels traces explorer
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-vdesk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null; cp "$WINEPREFIX/drive_c/sgswitch.log" "${ARTIFACTS:-/nonexistent}/" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
# programs join the shell's desktop, as in a session (sg-session's sg-run-explorer)
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { echo "\$*" >> "$T/log.out"; "$WINE" vdesk-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
shot() { sleep 1.5; import -window root "$T/\$1.png"; }
WINEDEBUG="${EXPLORER_DEBUG:-err+all},trace+explorer" "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
# wait until this explorer runs the desktop: a program started before would
# start an explorer of its own for it (and this one would exit)
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
# a tool window (as Start and flyouts are): no taskbar button
"$WINE" vdesk-probe.exe toolwindow Tool 700 400 >/dev/null 2>&1 &
sleep 2
"$WINE" vdesk-probe.exe window Alpha 60 60 >/dev/null 2>&1 &
sleep 2
"$WINE" vdesk-probe.exe window Beta 520 200 >/dev/null 2>&1 &
sleep 3
P query; P taskbar; shot one; ps -eo pid,args | grep -i "explorer" | grep -v grep > "$T/ps.out"
P foreground
# Alt+Tab on the X keyboard, as a person's keys arrive (through XWayland in a
# session): injected (SendInput) keys are not down in X, and winex11 releases
# them when focus moves -- which would let go of Alt at once
xdotool keydown alt key Tab; sleep 0.8; import -window root "$T/alttab.png"; P exists SgTaskSwitcher
xdotool keyup alt; sleep 0.8; P exists SgTaskSwitcher; P foreground
P hotkey new; sleep 2
P query; P state Alpha; P state Beta; P api Alpha; P taskbar; shot two
"$WINE" vdesk-probe.exe window Gamma 200 120 >/dev/null 2>&1 &
sleep 3
P state Gamma; P api Gamma; P taskbar
P hotkey left; sleep 2
P query; P state Alpha; P state Gamma; P api Gamma; P taskbar; shot back
P hotkey moveright; sleep 2
P query; P state Beta; P state Alpha
P hotkey taskview; sleep 2
P exists SgTaskView; shot taskview
P key 27; sleep 1
P exists SgTaskView
P hotkey close; sleep 2
P query; P state Alpha; P state Beta; P state Gamma; P taskbar; shot closed
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
"$WINE" reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops' 2>/dev/null | tr -d '\r' > "$T/reg.out"
sed 's/^/      /' "$T/log.out"

# the n-th answer after a command line in the log
after() { awk -v c="$1" -v n="${2:-1}" '$0 == c { k++; if (k == n) { getline; print; exit } }' "$T/log.out"; }
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }

[ "$(after query 1)" = "desktops=1 current=0" ] && pass "one desktop to start with" || fail "start: $(after query 1)"
[ "$(after taskbar 1)" = "buttons=2" ] && pass "both windows on the taskbar, and not the tool window" || fail "taskbar: $(after taskbar 1)"
[ "$(px one 200 200)" = "255,255,255" ] && pass "Alpha is on the screen" || fail "Alpha not drawn: $(px one 200 200)"
[ "$(after foreground 1)" = "foreground=Beta" ] || fail "Beta should be in front: $(after foreground 1)"
[ "$(after 'exists SgTaskSwitcher' 1)" = "exists=1" ] && pass "Alt+Tab shows the switcher while Alt is held" || fail "no switcher on Alt+Tab"
{ [ "$(after 'exists SgTaskSwitcher' 2)" = "exists=0" ] && [ "$(after foreground 2)" = "foreground=Alpha" ]; } \
    && pass "releasing Alt brings the previous window forward" || fail "after Alt+Tab: $(after 'exists SgTaskSwitcher' 2) $(after foreground 2)"

[ "$(after query 2)" = "desktops=2 current=1" ] && pass "Win+Ctrl+D makes desktop 2 and goes there" || fail "Win+Ctrl+D: $(after query 2)"
case "$(after 'state Alpha' 1)" in "visible=1 cloaked=0x2 desktop=0") pass "Alpha stays on desktop 1: shell-cloaked, still visible to its program" ;; *) fail "Alpha on desktop 2: $(after 'state Alpha' 1)" ;; esac
[ "$(px two 200 200)" != "255,255,255" ] && pass "and it is gone from the screen" || fail "Alpha still drawn on desktop 2"
case "$(after 'api Alpha' 1)" in "on_current=0 hr=0 "*) pass "IVirtualDesktopManager: Alpha is not on the current desktop" ;; *) fail "API for Alpha: $(after 'api Alpha' 1)" ;; esac
case "$(after 'api Alpha' 1)" in *"move_other=0x80070005") pass "MoveWindowToDesktop refuses another program's window" ;; *) fail "MoveWindowToDesktop: $(after 'api Alpha' 1)" ;; esac
[ "$(after taskbar 2)" = "buttons=0" ] && pass "the taskbar shows none of desktop 1's windows" || fail "taskbar on desktop 2: $(after taskbar 2)"
case "$(after 'state Gamma' 1)" in "visible=1 cloaked=0 desktop=1") pass "a window opened on desktop 2 is on desktop 2" ;; *) fail "Gamma: $(after 'state Gamma' 1)" ;; esac
case "$(after 'api Gamma' 1)" in "on_current=1 hr=0 "*) pass "and the API says it is on the current desktop" ;; *) fail "API for Gamma: $(after 'api Gamma' 1)" ;; esac
[ "$(after taskbar 3)" = "buttons=1" ] && pass "and it is on the taskbar" || fail "taskbar with Gamma: $(after taskbar 3)"

[ "$(after query 3)" = "desktops=2 current=0" ] && pass "Win+Ctrl+Left goes back to desktop 1" || fail "Win+Ctrl+Left: $(after query 3)"
case "$(after 'state Alpha' 2)" in "visible=1 cloaked=0 desktop=0") pass "Alpha is back" ;; *) fail "Alpha back: $(after 'state Alpha' 2)" ;; esac
case "$(after 'state Gamma' 2)" in "visible=1 cloaked=0x2 desktop=1") pass "Gamma is cloaked on desktop 2" ;; *) fail "Gamma away: $(after 'state Gamma' 2)" ;; esac
[ "$(px back 200 200)" = "255,255,255" ] && pass "Alpha is drawn again" || fail "Alpha not redrawn: $(px back 200 200)"
case "$(after 'api Gamma' 2)" in "on_current=0 hr=0 "*) pass "the API follows the switch" ;; *) fail "API after switch: $(after 'api Gamma' 2)" ;; esac
[ "$(after taskbar 4)" = "buttons=2" ] && pass "the taskbar shows desktop 1's windows again" || fail "taskbar back: $(after taskbar 4)"

[ "$(after query 4)" = "desktops=2 current=1" ] && pass "Win+Ctrl+Shift+Right goes to desktop 2" || fail "Win+Ctrl+Shift+Right: $(after query 4)"
b=$(after 'state Beta' 2); a=$(after 'state Alpha' 3)
{ [ "$b" = "visible=1 cloaked=0 desktop=1" ] && [ "$a" = "visible=1 cloaked=0x2 desktop=0" ]; } || \
{ [ "$a" = "visible=1 cloaked=0 desktop=1" ] && [ "$b" = "visible=1 cloaked=0x2 desktop=0" ]; } \
    && pass "carrying the active window along, and only it" || fail "carry: Alpha $a / Beta $b"

[ "$(after 'exists SgTaskView' 1)" = "exists=1" ] && pass "Win+Tab opens Task View" || fail "Task View did not open"
[ "$(px taskview 5 400)" = "32,32,32" ] && pass "Task View is on the screen" || fail "Task View not drawn: $(px taskview 5 400)"
[ "$(after 'exists SgTaskView' 2)" = "exists=0" ] && pass "Escape closes it" || fail "Escape did not close Task View"

grep -q 'VirtualDesktopIDs.*REG_BINARY' "$T/reg.out" && grep -q 'CurrentVirtualDesktop.*REG_BINARY' "$T/reg.out" \
    && pass "the state is in HKCU\\...\\Explorer\\VirtualDesktops, as on Windows" || fail "registry: $(cat "$T/reg.out")"

[ "$(after query 5)" = "desktops=1 current=0" ] && pass "Win+Ctrl+F4 closes the desktop" || fail "Win+Ctrl+F4: $(after query 5)"
all=1; for w in Alpha Beta Gamma; do n=$(grep -c "^state $w\$" "$T/log.out"); case "$(after "state $w" "$n")" in "visible=1 cloaked=0 desktop=0") ;; *) all=0 ;; esac; done
[ $all = 1 ] && pass "its windows come back to the remaining desktop" || fail "windows after close: $(grep -A1 '^state' "$T/log.out" | tail -6 | tr '\n' ' ')"
[ "$(after taskbar 5)" = "buttons=3" ] && pass "all three on the taskbar" || fail "taskbar after close: $(after taskbar 5)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
