#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The foreground lock (wine-sg 9930-9932), as Windows' SetForegroundWindow rules: a program the person
# is not working with cannot take the foreground and the keyboard; its taskbar button flashes orange.
# On a shell desktop, with the X keyboard and mouse (xdotool) as the person:
#   - FgA is clicked and typed into; FgB, started from outside (as over ssh), shows behind FgA, its
#     SetForegroundWindow fails, what is typed stays in FgA, and FgB's taskbar button turns orange
#     (and stays orange after its blinks); it stays under FgA on the screen and in Wine's Z order, also
#     when a compositor exposes all of it
#   - AllowSetForegroundWindow from the foreground process lets FgB come forward; a grant ends when
#     the person clicks another program (FgE) first
#   - a program the foreground one starts comes forward (FgC), also through a Unix bridge that starts it
#     again (FgH, as Setup's) or a launcher gone by then (FgI); LockSetForegroundWindow keeps even
#     that one back (FgD) until unlocked; an open menu keeps the last input's program (FgE) back
#   - a program in the background cannot turn the lock off (SPI_SETFOREGROUNDLOCKTIMEOUT); after
#     ForegroundLockTimeout without input a new program comes forward (FgF)
#   - Alt+Tab (the shell's hotkey) and a click on a taskbar button still bring a window forward, and
#     the click ends the button's flash
#   WINE=/opt/wine-sg/bin/wine test/fglock-gate.sh   (WINESERVER=... for a build tree; ARTIFACTS=DIR)
# Mutants: SG_MUTANT_FG_NO_LOCK, SG_MUTANT_FG_GRANTS_KEPT, SG_MUTANT_FG_EXPOSE_RAISE, SG_MUTANT_FG_NO_TOKEN (with NO_INHERIT),
# SG_MUTANT_FG_NO_INHERIT (server),
# SG_MUTANT_FG_ON_TOP, SG_MUTANT_FG_NO_FLASH, SG_MUTANT_FG_CLICK_ACTIVE (win32u), SG_MUTANT_FG_X_ON_TOP,
# SG_MUTANT_FG_MAP_FOCUS (winex11), SG_MUTANT_TASKBAR_NO_FLASH (explorer). (The probe's windows take the
# focus before they are shown, as Firefox's does.)
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
for t in Xvfb xdotool import python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done

T=$(mktemp -d /var/tmp/sg-fglock.XXXXXX)
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
cp "$T/fglock-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1
"$WINE" explorer /desktop=shell,1280x800 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3

P() { "$WINE" fglock-probe.exe "$@" 2>/dev/null | tr -d '\r'; }
W() { "$WINE" fglock-probe.exe win "$@" >/dev/null 2>&1 & sleep 3; }
C() { P cmd "$@"; sleep 1.2; P result "$1"; }
Q() { q=$(P query "$@"); echo "query: $q" >> "$T/log.out"; }
click() { xdotool mousemove "$1" "$2" click 1; sleep 1; }
# the colour at a point: "r g b"
pix() { import -window root "$T/shot.png" 2>/dev/null
        python3 -c "from PIL import Image; import sys; print(*Image.open(sys.argv[1]).convert('RGB').getpixel((int(sys.argv[2]), int(sys.argv[3]))))" "$T/shot.png" "$1" "$2"; }
orange() { set -- $1; [ "$1" -ge 200 ] && [ "$2" -ge 60 ] && [ "$2" -le 150 ] && [ "$3" -le 80 ]; }
# a point inside FgB's taskbar button, away from its icon and title
bpoint() { set -- $(P button FgB); [ $# = 4 ] && echo "$(( $1 + $3 - 6 )) $(( $2 + 5 ))"; }

# --- a program started in the background stays behind the one the person works in ---
W FgA 100 80
click 310 230; xdotool type --delay 40 abc; sleep 0.8
Q FgA
case "$q" in "fg=FgA | FgA text=abc"*) pass "the person's window FgA has the foreground and their keys ($q)" ;;
    *) fail "FgA, clicked and typed into: $q" ;; esac
W FgB 400 150
Q FgA FgB
case "$q" in "fg=FgA | FgA text=abc active=1 over=FgB,"*) \
    pass "FgB, started from outside while the person typed in FgA, shows behind FgA without the foreground ($q)" ;;
    *) fail "FgB started in the background: $q (want fg=FgA, FgA over FgB)" ;; esac
r=$(C FgB setfg)
[ "$r" = 0 ] && pass "FgB's own SetForegroundWindow fails (returned $r)" || fail "FgB's SetForegroundWindow returned $r (want 0)"
xdotool type --delay 40 xyz; sleep 0.8
Q FgA FgB
case "$q" in "fg=FgA | FgA text=abcxyz"*"FgB text= "*) pass "what the person types next still goes to FgA ($q)" ;;
    *) fail "typing after FgB came: $q (want FgA text=abcxyz, FgB empty)" ;; esac
# where they overlap X shows FgA (white inside), not FgB (black inside) over it
c=$(pix 460 260); set -- $c
[ "$1" -ge 200 ] && [ "$2" -ge 200 ] && [ "$3" -ge 200 ] && pass "on the screen too FgB is under FgA where they overlap ($c)" \
    || fail "where FgA and FgB overlap the screen shows $c (want FgA's white, not FgB's black)"
# under a compositor (sg-deskcomp redirects the desktop's X windows) an exposure covers all of FgB:
# it must not bring FgB over FgA
if command -v cc >/dev/null && [ -f /usr/include/X11/extensions/Xcomposite.h ] &&
   cc -O2 -o "$T/zowned-xhelper" "$HERE/zowned-xhelper.c" -lX11 -lXcomposite 2>/dev/null; then
    DESKX=$(xdotool search --name 'shell - Wine Desktop' 2>/dev/null | head -1)
    "$T/zowned-xhelper" redirect "$DESKX" > "$T/helper.out" 2>&1 & HP=$!
    i=0; while ! grep -q redirected "$T/helper.out" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
    "$T/zowned-xhelper" expose "$(P xwin FgB)"; sleep 1.5
    Q FgA FgB
    case "$q" in "fg=FgA | FgA text=abcxyz active=1 over=FgB,"*) pass "a composited exposure of all of FgB leaves it under FgA ($q)" ;;
        *) fail "FgB exposed under a compositor: $q (want FgA over FgB)" ;; esac
    kill $HP 2>/dev/null; wait $HP 2>/dev/null; sleep 1
else echo "SKIP  the compositor check (needs cc and libxcomposite-dev)"
fi
bp=$(bpoint)
if [ -z "$bp" ]; then fail "no taskbar button for FgB"
else
    # three blinks (half a second each way), then lit for good: sampled lit within the first second
    lit=0; for k in 1 2 3 4; do c=$(pix $bp); orange "$c" && lit=1; sleep 0.25; done
    [ $lit = 1 ] && pass "FgB's taskbar button flashes orange ($c at $bp)" || fail "FgB's taskbar button never orange ($c at $bp)"
    sleep 3.5; c=$(pix $bp); cp "$T/shot.png" "$T/flash-steady.png"
    orange "$c" && pass "after its blinks FgB's button stays orange ($c)" || fail "FgB's button after the blinks: $c (want orange)"
fi

# --- AllowSetForegroundWindow ---
r=$(C FgB allow FgA)
[ "$r" = 0 ] && pass "FgB in the background cannot AllowSetForegroundWindow (returned $r)" || fail "FgB's AllowSetForegroundWindow returned $r (want 0)"
r=$(C FgA allow FgB)
[ "$r" = 1 ] && pass "FgA, in front, may AllowSetForegroundWindow(FgB)" || fail "FgA's AllowSetForegroundWindow returned $r"
r=$(C FgB setfg); Q FgA FgB
case "$r $q" in "1 fg=FgB"*) pass "with FgA's grant FgB comes forward ($q)" ;; *) fail "FgB with a grant: $r $q" ;; esac
click 310 230; Q FgA
case "$q" in "fg=FgA"*) ;; *) fail "a click on FgA: $q" ;; esac
r=$(C FgA allow FgB)
W FgE 100 440
[ -n "${ARTIFACTS:-}" ] && import -window root "$T/fge.png" 2>/dev/null
click 310 600; xdotool type --delay 40 e; sleep 0.8; Q FgE
case "$q" in "fg=FgE | FgE text=e "*) pass "FgE, kept behind, comes forward on a click and takes the keys ($q)" ;;
    *) fail "a click on FgE, then e typed: $q (want FgE in front with the e)" ;; esac
r=$(C FgB setfg); Q FgE FgB
case "$r $q" in "0 fg=FgE"*) pass "a grant ends once the person clicks another program: FgB stays back ($q)" ;;
    *) fail "FgB's grant after the person moved to FgE: $r $q (want refused)" ;; esac

# --- started by the foreground program; LockSetForegroundWindow; menus ---
r=$(C FgE spawn FgC); sleep 2; Q FgE FgC
case "$q" in "fg=FgC"*) pass "a program FgE (in front) starts comes forward ($q)" ;; *) fail "FgC started by FgE: $q" ;; esac
r=$(C FgC lock)
[ "$r" = 1 ] && pass "FgC, in front, takes LockSetForegroundWindow" || fail "FgC's LockSetForegroundWindow returned $r"
r=$(C FgC spawn FgD); sleep 2; Q FgC FgD
case "$q" in "fg=FgC"*) pass "while FgC holds the lock even a program it starts stays back ($q)" ;; *) fail "FgD under the lock: $q" ;; esac
r=$(C FgC unlock); r=$(C FgD setfg); Q FgC FgD
case "$r $q" in "1 fg=FgD"*) pass "unlocked, FgD (FgC's) comes forward ($q)" ;; *) fail "FgD after the unlock: $r $q" ;; esac
P cmd FgD menu; sleep 1.5
r=$(C FgE setfg); Q FgD FgE
case "$r $q" in "0 fg=FgD"*) pass "while FgD's menu is open, FgE (the last clicked) stays back ($q)" ;; *) fail "FgE during FgD's menu: $r $q" ;; esac
P cmd FgD endmenu; sleep 1
r=$(C FgE setfg); Q FgE
case "$r $q" in "1 fg=FgE"*) pass "the menu closed, FgE (the person's last click) may come forward ($q)" ;; *) fail "FgE after the menu: $r $q" ;; esac

# --- ForegroundLockTimeout ---
r=$(C FgB timeout 2000)
[ "$r" = 0 ] && pass "FgB in the background cannot change ForegroundLockTimeout" || fail "FgB set ForegroundLockTimeout ($r)"
r=$(C FgE timeout 2000)
[ "$r" = 1 ] || fail "FgE (in front) setting ForegroundLockTimeout: $r"
sleep 3
W FgF 640 440; Q FgE FgF
case "$q" in "fg=FgF"*) pass "after ForegroundLockTimeout without input a new program comes forward ($q)" ;; *) fail "FgF after the lock's time-out: $q" ;; esac
r=$(C FgF timeout 200000)
# ASFW_ANY from the program in front; one whose parent is gone by the time it shows its window (a program
# started through start.exe, a launcher's; a parent that no longer exists is no error) comes forward with it
r=$(C FgF allow any)
"$WINE" fglock-probe.exe launch FgG 700 200 3000 >/dev/null 2>&1; sleep 6; Q FgF FgG
case "$r $q" in "1 fg=FgG"*) pass "with ASFW_ANY a program whose parent is gone comes forward ($q)" ;;
    *) fail "FgG (its parent gone, under ASFW_ANY): $r $q" ;; esac
# a program the person may bring forward starts the real window's program through a Unix program (as
# Setup does through its bridge) and is gone: the window comes forward all the same
r=$(C FgG spawn bridge:FgH); sleep 6; Q FgG FgH
case "$q" in "fg=FgH"*) pass "a window started through a Unix bridge by a program in front comes forward ($q)" ;;
    *) fail "FgH, started through a Unix bridge by FgG (in front): $q" ;; esac
# ... and through a launcher that is gone before the window shows (an installer's stub, a .lnk's helper)
r=$(C FgH spawn stub:FgI); sleep 5; Q FgH FgI
case "$q" in "fg=FgI"*) pass "a window started through a launcher that already exited comes forward ($q)" ;;
    *) fail "FgI, started by FgH (in front) through a launcher gone since: $q" ;; esac

# --- the person's own ways still work: Alt+Tab, a taskbar button ---
click 310 600; Q FgE
xdotool keydown alt; sleep 0.2; xdotool key Tab; sleep 0.4; xdotool keyup alt; sleep 1.5; Q FgE FgF
case "$q" in "fg=FgE"*) fail "Alt+Tab from FgE left FgE in front ($q)" ;;
    "fg=Fg"[A-I]" "*) pass "Alt+Tab (the shell's switcher) brings another window forward ($q)" ;;
    *) fail "Alt+Tab from FgE: $q" ;; esac
bp=$(bpoint)
if [ -n "$bp" ]; then
    click $bp; Q FgB FgA
    case "$q" in "fg=FgB | FgB text= active=1 over=FgA,"*) pass "a click on FgB's taskbar button brings it forward, over FgA ($q)" ;;
        *) fail "FgB's taskbar button clicked: $q (want FgB in front, over FgA)" ;; esac
    xdotool mousemove 640 300; sleep 0.5; c=$(pix 460 260); set -- $c
    [ "$1" -le 40 ] && [ "$2" -le 40 ] && [ "$3" -le 40 ] && pass "on the screen too FgB is over FgA now ($c)" \
        || fail "FgB brought forward is not over FgA on the screen ($c, want FgB's black)"
    xdotool mousemove 640 300; sleep 0.8; c=$(pix $bp)
    orange "$c" && fail "FgB's button still orange once it is in front ($c)" || pass "in front, FgB's button is no longer orange ($c)"
fi
exit $RC
