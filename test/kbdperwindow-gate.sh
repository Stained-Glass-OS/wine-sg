#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# "Let me use a different input method for each app window" (patches/sg/9161; ledger i0564). Windows keeps the
# keyboard the person's whatever window they type in, unless that setting is on (SPI_SETTHREADLOCALINPUTSETTINGS,
# UserPreferencesMask byte 4 0x80): then a window coming to the front brings its own layout -- the one its thread
# last had, or the first for a thread that never switched -- and the keyboard switches to it.
# An Xvfb with the keymap "us,de" and grp:win_space_toggle (test/kbdxtool.c), the shell's desktop, two windows of
# test/kbdperwindow-probe.c in two processes, A and B, real keys (XTEST) and clicks (xdotool):
#   1. per window: SPI set and read back; Win+Space in A (German, X group 1); a click on B: B's own, the first
#      (US): WM_INPUTLANGCHANGE 04090409 and the X keyboard back to group 0; a click on A: German again (group 1)
#   2. the default (global): Win+Space in A (German); a click on B: the keyboard stays German (group 1)
#   WINE=/opt/wine-sg/bin/wine test/kbdperwindow-gate.sh
# Mutant: SG_MUTANT_KBD_GLOBAL_ONLY (win32u/input.c) fails 1.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDPERWINDOW_DPY:-478}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""; EP=""; AP=""; BP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo xkbcomp xdotool cc "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/de ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
[ -f /usr/include/xkbcommon/xkbcommon.h ] && [ -f /usr/include/X11/extensions/XTest.h ] || { echo "SKIP: no libxkbcommon-dev/libxtst-dev"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdperwindow.XXXXXX)
cleanup() {
    for p in $AP $BP $EP; do kill "$p" 2>/dev/null; done
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$T/prefix"
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    sleep 1
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
cc -O2 -o "$T/kbdxtool" "$HERE/kbdxtool.c" -lxkbcommon -lXtst -lX11 || { echo "FAIL  kbdxtool did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/kbdperwindow-probe.c" -luser32 || { echo "FAIL  the probe did not build"; exit 1; }
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
K="$T/kbdxtool keys"
SWITCH="+133 s100 +65 s100 -65 s100 -133 s800"
rc=0
expect() { [ "$2" = "$3" ] && echo "PASS  $1" || { echo "FAIL  $1: got '$2', want '$3'"; rc=1; }; }
has() { case "$(tr -d '\r' < "$T/out" | tr '\n' '|')" in *"$2"*) echo "PASS  $1" ;; *) echo "FAIL  $1: want '$2'"; rc=1 ;; esac; }
run() {   # run MODE: A at x 50, B at x 500
    : > "$T/out"
    timeout -s KILL 90 "$WINE" "$T/probe.exe" A 50 "$1" >> "$T/out" 2>/dev/null </dev/null & AP=$!
    i=0; while ! grep -q 'A READY' "$T/out" && [ $i -lt 80 ]; do sleep 0.25; i=$((i + 1)); done
    timeout -s KILL 90 "$WINE" "$T/probe.exe" B 500 >> "$T/out" 2>/dev/null </dev/null & BP=$!
    i=0; while ! grep -q 'B READY' "$T/out" && [ $i -lt 80 ]; do sleep 0.25; i=$((i + 1)); done
    sleep 1
    xdotool mousemove 200 150 click 1; sleep 1          # A in front
    $K +38 -38 s300 $SWITCH +38 -38 s600                # a key, Win+Space, a key: A takes German
    g1=$(group)
    xdotool mousemove 650 150 click 1; sleep 1.5        # B in front
    $K +38 -38 s300
    g2=$(group)
    xdotool mousemove 200 150 click 1; sleep 1.5        # A again
    $K +38 -38 s300
    g3=$(group)
    $K +24 -24 s300; xdotool mousemove 650 150 click 1; sleep 0.8; $K +24 -24 s500
    wait "$AP" "$BP" 2>/dev/null; AP=""; BP=""
    sed 's/^/      /' "$T/out" | tr -d '\r'
}
echo "1. a different keyboard for each app window"
run local
has "SPI_SETTHREADLOCALINPUTSETTINGS on, read back" "A SPI 1 1"
expect "Win+Space in A: German" "$g1" 1
has "a click on B while A types German: B types with its own layout, the first (US)" "A CHANGE 04070409|B ACTIVE|B NOW 04090409"
expect "...the keyboard switches to US" "$g2" 0
expect "a click on A: A's German again" "$g3" 1
echo "2. the default: the keyboard is the person's"
"$T/kbdxtool" keys $SWITCH >/dev/null 2>&1; [ "$(group)" = 0 ] || "$T/kbdxtool" keys $SWITCH
run global
has "SPI off, read back" "A SPI 1 0"
expect "Win+Space in A: German" "$g1" 1
expect "a click on B: the keyboard stays German" "$g2" 1
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
