#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's input indicator, as Windows 10's (patches/sg/9160, 9162; ledger i0564). With two keyboards the
# taskbar shows the input language left of the clock, lists the keyboards on a click and picks one; the
# Windows logo key + Space shows the same list while the key is held; the indicator follows switches made by
# the keyboard and by programs (HSHELL_LANGUAGE, which ActivateKeyboardLayout of the foreground thread sends).
# Wine had none of it: no indicator, no HSHELL_LANGUAGE.
# An Xvfb is given the compositor's keymap "us,de" with grp:win_space_toggle the way Xwayland is
# (test/kbdxtool.c), the shell's taskbar runs (explorer /desktop=shell) and test/inputind-probe.c's window is
# in the foreground; real key events go to it with XTEST, the pointer with xdotool:
#   1. the indicator is there, "ENG US" (two English keyboards: the keyboard under the language)
#   2. Win+Space: the list is up while the key is held ("Switching input"), gone after; the indicator
#      "ENG DE"; the program got the request (9154) and the shell was told (SHELL 04070409)
#   3. the program activates US: the indicator follows (HSHELL_LANGUAGE)
#   4. a background thread activates German: the indicator does not move, no HSHELL_LANGUAGE
#   5. a click on the indicator lists the keyboards ("Input"); a click on German: the keyboard types German
#      (the X keyboard's group 1, the Y key VK_Z), the program was asked (CHANGE 04070409), the indicator
#      "ENG DE", the list gone
#   WINE=/opt/wine-sg/bin/wine test/inputind-gate.sh   (INPUTIND_DPY=N: Xvfb :N, default 476)
# Mutants: SG_MUTANT_KBD_NO_TELL_SHELL (win32u/input.c) fails 3; SG_MUTANT_IND_IGNORE_SHELL 2,
# SG_MUTANT_IND_NO_HOTKEY 2, SG_MUTANT_IND_PICK_NOTHING 3, SG_MUTANT_IND_NO_ROOM 7 (programs/explorer/inputind.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${INPUTIND_DPY:-476}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""; PP=""; EP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo xkbcomp xdotool cc "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/de ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
[ -f /usr/include/xkbcommon/xkbcommon.h ] && [ -f /usr/include/X11/extensions/XTest.h ] || { echo "SKIP: no libxkbcommon-dev/libxtst-dev"; exit 77; }
T=$(mktemp -d /var/tmp/sg-inputind.XXXXXX)
cleanup() {
    [ -n "$PP" ] && kill "$PP" 2>/dev/null
    [ -n "$EP" ] && kill "$EP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$T/prefix"
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    sleep 1
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
cc -O2 -o "$T/kbdxtool" "$HERE/kbdxtool.c" -lxkbcommon -lXtst -lX11 || { echo "FAIL  kbdxtool did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/inputind-probe.c" -luser32 || { echo "FAIL  the probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp -noreset >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
"$T/kbdxtool" keymap us,de "" grp:win_space_toggle > "$T/keymap.xkb" && xkbcomp -w0 "$T/keymap.xkb" "$DISPLAY" 2>/dev/null
"$T/kbdxtool" group | grep -q "name 1 German" || { echo "FAIL  the precondition: no German second group"; exit 1; }
group() { "$T/kbdxtool" group | sed -n 's/^group //p'; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & EP=$!
sleep 8
timeout -s KILL 120 "$WINE" "$T/probe.exe" > "$T/out" 2>/dev/null </dev/null & PP=$!
i=0; while ! grep -q '^READY' "$T/out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.25; i=$((i + 1)); done
sleep 1
K="$T/kbdxtool keys"
log() { tr -d '\r' < "$T/out"; }
mark() { log | wc -l; }
since() { log | tail -n +"$(( $1 + 1 ))" | tr '\n' '|'; }
rc=0
has() {  # has WHAT TEXT PATTERN
    case "$2" in *"$3"*) echo "PASS  $1" ;; *) echo "FAIL  $1: want '$3'"; rc=1 ;; esac
}
hasnt() {
    case "$2" in *"$3"*) echo "FAIL  $1: did not want '$3'"; rc=1 ;; *) echo "PASS  $1" ;; esac
}
start=$(log | tr '\n' '|')
has "the indicator is shown with two keyboards, 'ENG US'" "$start" 'IND 1 "ENG US" 04090409'
set -- $(log | sed -n 's/^INDRECT //p')
if [ $# = 4 ] && [ $(( $3 - $1 )) -ge 20 ] && [ "$4" -le 768 ]; then
    echo "PASS  ...left of the clock, room of its own ($1,$2-$3,$4)"; IX=$(( ($1 + $3) / 2 )); IY=$(( ($2 + $4) / 2 ))
else echo "FAIL  the indicator's place: '$*'"; rc=1; IX=0; IY=0; fi

# 2. Win+Space
m=$(mark)
$K +133 s150 +65 s100 -65 s800
held=$(since "$m")
$K -133 s1200
after=$(since "$m")
has "Win+Space: the list is up while the Windows logo key is held" "$held" 'FLY 1 "Switching input"'
has "...gone when it is let go" "$after" 'FLY 0'
has "...the program was asked for German (9154)" "$after" 'REQ 3 04070409|CHANGE 04070409|'
has "...the shell was told (HSHELL_LANGUAGE)" "$after" 'SHELL 04070409'
has "...the indicator: 'ENG DE'" "$after" 'IND 1 "ENG DE" 04070409'

# 3. the program activates US (key a)
m=$(mark)
$K +38 -38 s1200
after=$(since "$m")
has "a program's ActivateKeyboardLayout(US) in the foreground: the indicator follows" "$after" 'IND 1 "ENG US" 04090409'

# 4. a background thread activates German (key b)
m=$(mark)
$K +56 -56 s1500
after=$(since "$m")
has "a background thread activates German" "$after" 'BACK 04070409'
hasnt "...the indicator does not move" "$after" 'IND 1 "ENG DE"'
hasnt "...the shell is not told" "$after" 'SHELL'

# 5. a click on the indicator, then on German
m=$(mark)
xdotool mousemove "$IX" "$IY"; sleep 0.3; xdotool click 1; sleep 1.2
opened=$(since "$m")
has "a click on the indicator lists the keyboards" "$opened" 'FLY 1 "Input"'
set -- $(log | sed -n 's/^FLY 1 "Input" //p' | tail -n 1)
if [ $# = 4 ]; then
    xdotool mousemove $(( $1 + 120 )) $(( $2 + 56 + 28 )); sleep 0.3; xdotool click 1; sleep 1.5
fi
picked=$(since "$m")
has "...a click on German: the list goes" "$picked" 'FLY 0'
has "...the indicator: 'ENG DE'" "$picked" 'IND 1 "ENG DE" 04070409'
has "...the program is asked to take it" "$picked" 'CHANGE 04070409'
g=$(group)
[ "$g" = 1 ] && echo "PASS  ...the keyboard types German (X group 1)" || { echo "FAIL  the keyboard's group: '$g'"; rc=1; }
m=$(mark)
$K +29 -29 s600
has "...the program is in front again and the Y key is VK_Z" "$(since "$m")" 'DOWN 5a'
$K +24 -24 s300
sed 's/^/      /' "$T/out" | tr -d '\r'
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
