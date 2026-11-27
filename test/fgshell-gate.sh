#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell under the foreground lock (wine-sg 9930-9932), with the X keyboard and mouse as the person,
# on a shell desktop, while a program (FgA) has the foreground and the person's keys:
#   - the Windows key alone opens Start in front: sg-start's stand-in (a hidden SgStartPanel window, as
#     sg-start has) shows its panel and its SetForegroundWindow succeeds -- the key is the shell's (as
#     Windows gives it to the taskbar) and explorer passes the foreground on to Start's program;
#     the Start button's click too
#   - Win+D shows the desktop with the keyboard on its icons, as Windows' desktop list view has it
#     (i0639: it was the taskbar, and keys did nothing): a letter selects the icon whose name starts
#     with it, the same letter again the next one, Enter opens it -- and the program it opens comes
#     forward (started by the shell on the person's key)
#   WINE=/opt/wine-sg/bin/wine test/fgshell-gate.sh   (WINESERVER=... for a build tree; ARTIFACTS=DIR)
# Mutants: SG_MUTANT_FG_NO_WIN_GRANT (server), SG_MUTANT_NO_PASS_FOREGROUND, SG_MUTANT_WIND_TASKBAR (explorer).
set -u
unset DISPLAY XAUTHORITY
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done

T=$(mktemp -d /var/tmp/sg-fgshell.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
XP=
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.out "$T"/*.png "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/fglock-probe.exe" "$HERE/fglock-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
"$WINESERVER" -w
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
DESK=$("$WINE" winepath -u "C:\\users\\$U\\Desktop" 2>/dev/null | tr -d '\r')
sg_prefix_safe "$WINEPREFIX" || exit 1
r=$(realpath -m "$DESK")
case "$r" in "$T"/*|"$SG_GATE_HOME"/*) ;; *) echo "FAIL  the Desktop is outside the gate's folders: $DESK"; exit 1 ;; esac
mkdir -p "$DESK"
echo one > "$DESK/alpha.txt"
echo two > "$DESK/bravo.txt"
echo three > "$DESK/bistro.txt"
cp "$T/fglock-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1
"$WINE" explorer /desktop=shell,1280x800 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3

P() { "$WINE" fglock-probe.exe "$@" 2>/dev/null | tr -d '\r'; }
Q() { q=$(P query "$@"); echo "query: $q" >> "$T/log.out"; }
click() { xdotool mousemove "$1" "$2" click 1; sleep 1; }

"$WINE" fglock-probe.exe startpanel >/dev/null 2>&1 &
"$WINE" fglock-probe.exe win FgA 500 120 >/dev/null 2>&1 &
sleep 4
click 700 280; xdotool type --delay 40 abc; sleep 0.8
Q FgA
case "$q" in "fg=FgA | FgA text=abc"*) ;; *) fail "FgA, clicked and typed into: $q" ;; esac

# --- Start: the Windows key alone, the Start button ---
xdotool key super; sleep 2
r=$(P result FgStart); Q FgA FgStart
case "$r $q" in "1 fg=FgStart"*) pass "the Windows key alone brings Start forward over FgA ($r $q)" ;;
    *) fail "the Windows key: Start's SetForegroundWindow $r, $q (want 1, fg=FgStart)" ;; esac
xdotool key super; sleep 1.5
click 700 280; Q FgA
case "$q" in "fg=FgA"*) ;; *) fail "back to FgA: $q" ;; esac
click 28 780; sleep 1
r=$(P result FgStart); Q FgA FgStart
case "$r $q" in "1 fg=FgStart"*) pass "a click on the Start button brings Start forward ($r $q)" ;;
    *) fail "the Start button: Start's SetForegroundWindow $r, $q (want 1, fg=FgStart)" ;; esac
click 28 780; sleep 1

# --- Win+D: the keyboard on the desktop's icons (i0639) ---
click 700 280; xdotool type --delay 40 d; sleep 0.5
xdotool key super+d; sleep 2
Q FgA
case "$q" in "fg=Desktop"*) pass "Win+D gives the keyboard to the desktop's icons ($q)" ;;
    *) fail "after Win+D the foreground is not the desktop: $q" ;; esac
xdotool type --delay 60 b; sleep 0.3; xdotool type --delay 60 b; sleep 0.3
[ -n "${ARTIFACTS:-}" ] && import -window root "$T/typed.png" 2>/dev/null
xdotool key Return; sleep 6
f=$(P find bistro); g=$(P find bravo)
# the icons by name: alpha, bistro, bravo -- b selects bistro, b again bravo
case "$f $g" in "0 fg=0 1 fg=1") pass "b, b again and Enter open the second icon starting with b (bravo.txt), in front ($f / $g)" ;;
    *) fail "typing b b Enter on the desktop: bistro $f, bravo $g (want bravo open and in front)" ;; esac
Q FgA
case "$q" in *"FgA text=abcd "*) pass "none of it went to FgA ($q)" ;; *) fail "FgA got keys meant for the desktop: $q" ;; esac
exit $RC
