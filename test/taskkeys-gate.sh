#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's keys (patches/sg/9068), typed on the X keyboard, as Windows has them:
#   Win+1..Win+9, Win+0: the taskbar's Nth button as if clicked -- a window comes forward, or
#     is minimized when it is the one in front;
#   Win+S, Win+Q: Start, ready for typing (search);
#   Win+Alt+D: the clock's calendar;
#   Win+Home: every window but the one in front minimized; again, back;
#   Win+P: the Project pane (ms-settings-displays-topology:projection, sg-shell's Settings);
#   the taskbar's menu: Show windows side by side, and its Undo (9069);
#   Win+X: Search and Shut down or sign out (9070; nothing in that submenu is chosen).
#
#   WINE=/opt/wine-sg/bin/wine test/taskkeys-gate.sh
# Mutants: explorer built with -DSG_MUTANT_WIN_NUMBER_NONE, -DSG_MUTANT_WIN_HOME_NONE, -DSG_MUTANT_MENU_NO_ARRANGE,
#   -DSG_MUTANT_WINX_NO_POWER.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs mingw-w64 and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskkeys.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 &&
"$MINGW" -O2 -o "$T/taskkeys-probe.exe" "$HERE/taskkeys-probe.c" || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
# Win+P's URI, as sg-shell's Settings registers it: a stand-in that notes it was started
"$WINE" reg add 'HKLM\Software\Classes\ms-settings-displays-topology' /v 'URL Protocol' /d '' /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\Classes\ms-settings-displays-topology\shell\open\command' /ve /d 'C:\taskkeys-probe.exe C:\project.log' /f >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/vdesk-probe.exe" "$T/taskkeys-probe.exe" "$C/"
cd "$C" || exit 1
P() { echo "$*" >> "$T/log.out"; timeout -s KILL 30 "$WINE" vdesk-probe.exe "$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
K() { echo "key $*" >> "$T/log.out"; xdotool key "$@"; sleep 1.5; }
WINEDEBUG=-all "$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
sleep 8
"$WINE" taskkeys-probe.exe 'C:\stand.log' >/dev/null 2>&1 &
"$WINE" vdesk-probe.exe window One 60 60 >/dev/null 2>&1 &
sleep 3
"$WINE" vdesk-probe.exe window Two 400 200 >/dev/null 2>&1 &
sleep 4
P taskbar
# a click on Two's button first: started from outside, it may have opened behind (9930)
P foreground
K super+1; P foreground; P rect One
K super+1; P rect One
K super+2; P foreground
# Win+Home: the window in front stays, the others go; again: back
K super+1; P foreground
K super+Home; P rect One; P rect Two
K super+Home; P rect Two
# the taskbar's menu (9069): Show windows side by side, then its Undo
# (an empty part of the taskbar: the bar is at the bottom of the screen the desktop fills)
BY=$(( $(xwininfo -root | awk '/Height:/ {print $2}') - 18 ))
xdotool mousemove 700 "$BY" click 3; sleep 1.5; K i
P rect One; P rect Two
xdotool mousemove 700 "$BY" click 3; sleep 1.5; [ -n "${SHOT:-}" ] && xwd -root -silent | convert xwd:- "$SHOT"; K u
P rect One
# Win+X (9070): Shut down or sign out opens its submenu (nothing in it is chosen here), Search
K super+x; K u
echo menus >> "$T/log.out"; timeout -s KILL 30 "$WINE" taskkeys-probe.exe menus 2>/dev/null | tr -d '\r' >> "$T/log.out"
K Escape; K Escape; K Escape
K super+x; K h
K super+s
K super+q
K super+alt+d
K super+p; sleep 2
sed 's/^/      /' "$T/log.out"
after() { awk -v c="$1" -v n="${2:-1}" '$0 == c { k++; if (k == n) { getline; print; exit } }' "$T/log.out"; }
[ "$(after foreground 2)" = "foreground=One" ] && pass "Win+1: the first taskbar button's window comes forward" \
    || fail "Win+1: $(after foreground 2) (want One)"
case "$(after 'rect One' 2)" in *"iconic=1") pass "Win+1 again, on the window in front: minimized, as a click on its button" ;;
    *) fail "Win+1 on the front window: $(after 'rect One' 2)" ;; esac
[ "$(after foreground 3)" = "foreground=Two" ] && pass "Win+2: the second button's window" || fail "Win+2: $(after foreground 3)"
case "$(after foreground 4) $(after 'rect One' 3) $(after 'rect Two' 1)" in
"foreground=One "*"iconic=0 "*"iconic=1") pass "Win+Home: the window in front stays, the others are minimized" ;;
*) fail "Win+Home: $(after foreground 4) | One $(after 'rect One' 3) | Two $(after 'rect Two' 1)" ;; esac
case "$(after 'rect Two' 2)" in *"iconic=0") pass "Win+Home again: they come back" ;; *) fail "Win+Home again: $(after 'rect Two' 2)" ;; esac
w1=$(after 'rect One' 4 | sed 's/^rect=//; s/ zoomed.*//'); w2=$(after 'rect Two' 3 | sed 's/^rect=//; s/ zoomed.*//')
IFS=, read -r l1 t1 r1 b1 <<R
$w1
R
IFS=, read -r l2 t2 r2 b2 <<R
$w2
R
half=$(( $(xwininfo -root | awk '/Width:/ {print $2}') / 2 ))
if [ -n "${r1:-}" ] && [ -n "${r2:-}" ] && [ "$((r1 - l1))" -ge "$((half - 20))" ] && [ "$((r2 - l2))" -ge "$((half - 20))" ] && [ "$((r1 - l1))" -le "$((half + 20))" ] &&
   { [ "$r1" -le "$((l2 + 4))" ] || [ "$r2" -le "$((l1 + 4))" ]; } && [ "$((b1 - t1))" -ge 600 ]; then
    pass "the taskbar's menu: Show windows side by side ($w1 | $w2)"
else fail "Show windows side by side: One $w1, Two $w2"; fi
[ "$(after 'rect One' 5)" = "$(after 'rect One' 1)" ] && pass "...and its Undo puts them back ($(after 'rect One' 5))" \
    || fail "Undo side by side: $(after 'rect One' 5) (was $(after 'rect One' 1))"
tr -d '\r' < "$C/stand.log" > "$T/stand.log" 2>/dev/null
n=$(grep -c '^start ready-for-typing 1$' "$T/stand.log")
[ "${n:-0}" = 3 ] && pass "Win+X > Search, Win+S and Win+Q: Start, ready for typing (search)" || fail "Win+X Search/Win+S/Win+Q: $n of 3: $(tr '\n' '|' < "$T/stand.log")"
[ "$(after menus)" = "menus=2" ] && pass "Win+X > Shut down or sign out: its submenu" || fail "Win+X > Shut down or sign out: $(after menus) (want 2 menus open)"
grep -q '^calendar toggle bar=1$' "$T/stand.log" && pass "Win+Alt+D: the clock's calendar, under the main taskbar's clock" \
    || fail "Win+Alt+D: $(tr '\n' '|' < "$T/stand.log")"
grep -q '^ready' "$C/project.log" 2>/dev/null && pass "Win+P: the Project pane (ms-settings-displays-topology:projection)" \
    || fail "Win+P: nothing started for ms-settings-displays-topology:"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
sg_kill_prefix "$WINEPREFIX"
[ $RC = 0 ] && echo "taskkeys-gate: all PASS" || echo "taskkeys-gate: FAIL"
exit $RC
