#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's ActivateKeyboardLayout of one of the session's layouts switches the keyboard (patches/sg/9156).
# On Windows the input method is the person's ("Let me use a different input method for each app window"
# is off by default): a program in the foreground that activates one of the loaded layouts switches the
# keyboard -- the keys type in it and the input indicator, which follows the focus window, shows it; a
# thread in the background changes its own layout only. The session's layouts are the display server's
# XKB groups (sg-compositor owns them on Xwayland), so Wine changed the thread's tables and the keyboard
# went on typing the old layout, and the person's indicator never moved.
# An Xvfb has the compositor's keymap "us,de" with grp:win_space_toggle (test/kbdxtool.c); keys go to
# test/kbdactivate-probe.c's window with XTEST.
#   1. no compositor (an X server of its own): the foreground thread activates German -> the X keyboard's
#      group is German and the Y key is VK_Z z with no input language request; a background thread
#      activating US then changes nothing of the keyboard's
#   2. a compositor (SG_LOCK_CONTROL, a stand-in that records what it is asked): the activation is
#      "LAYOUT 1" to it and the X server's group is left to it; two switches of the person's own, queued
#      while the program is busy, are not sent back to it as switches of the program's
#   WINE=/opt/wine-sg/bin/wine test/kbdactivate-gate.sh   (KBDACTIVATE_DPY=N: Xvfb :N, default 500)
# Mutants (winex11.drv): SG_MUTANT_KBD_ACTIVATE_LOCAL (the thread's layout only), SG_MUTANT_KBD_ACTIVATE_ANY_THREAD
# (a background thread switches too), SG_MUTANT_KBD_REQUEST_SWITCHES (the person's own switches echoed).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDACTIVATE_DPY:-500}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""; PP=""; SP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo xkbcomp xdotool cc python3 "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/de ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
[ -f /usr/include/xkbcommon/xkbcommon.h ] && [ -f /usr/include/X11/extensions/XTest.h ] || { echo "SKIP: no libxkbcommon-dev/libxtst-dev"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdactivate.XXXXXX)
cleanup() {
    [ -n "$PP" ] && kill "$PP" 2>/dev/null
    [ -n "$SP" ] && kill "$SP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
cc -O2 -o "$T/kbdxtool" "$HERE/kbdxtool.c" -lxkbcommon -lXtst -lX11 || { echo "FAIL  kbdxtool did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/kbdactivate-probe.c" -luser32 || { echo "FAIL  the probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 800x600x24 -nolisten tcp -noreset >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
"$T/kbdxtool" keymap us,de "" grp:win_space_toggle > "$T/keymap.xkb" && xkbcomp -w0 "$T/keymap.xkb" "$DISPLAY" 2>/dev/null
"$T/kbdxtool" group | grep -q "name 1 German" || { echo "FAIL  the precondition: no German second group"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
group() { "$T/kbdxtool" group | sed -n 's/^group \([0-9]*\).*/\1/p' | head -n 1; }
K="$T/kbdxtool keys"
SWITCH="+133 s60 +65 s60 -65 s60 -133 s60"
check() {  # check WHAT PATTERN: PATTERN (lines joined by |) must be in the sequence
    case "$(cat "$T/seq")" in *"$2"*) echo "PASS  $1" ;; *) echo "FAIL  $1: want '$2'"; rc=1 ;; esac
}
expect() {  # expect WHAT GOT WANT
    if [ "$2" = "$3" ]; then echo "PASS  $1"; else echo "FAIL  $1: got '$2', want '$3'"; rc=1; fi
}
start_probe() {
    rm -f "$T/out"
    timeout -s KILL 90 "$WINE" "$T/probe.exe" > "$T/out" 2>/dev/null </dev/null & PP=$!
    i=0; while ! grep -q '^READY' "$T/out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.25; i=$((i + 1)); done
    WID=$(timeout 20 xdotool search --sync --name '^sgkbdactivate$' | head -n 1)
    [ -n "$WID" ] && { xdotool windowactivate --sync "$WID" 2>/dev/null || xdotool windowfocus --sync "$WID"; }
    sleep 1
}
finish_probe() {
    i=0; while ! grep -q '^DONE\|^TIMEOUT' "$T/out" && [ $i -lt 80 ]; do sleep 0.25; i=$((i + 1)); done
    wait "$PP" 2>/dev/null; PP=""
    tr -d '\r' < "$T/out" > "$T/log"
    sed 's/^/      /' "$T/log"
    grep -v '^DOWN 5b\|^DOWN 20\|^CHAR 0020\|^READY' "$T/log" | tr '\n' '|' > "$T/seq"
}

# --- 1. no compositor: the X server's own group ---
echo "1. an X server of its own"
start_probe
$K +24 -24 s800                      # Q: the foreground thread activates German
g1=$(group)
$K +29 -29 s800                      # the Y key: German z, on which a background thread activates US
g2=$(group)
$K +29 -29 s400 +26 -26 s400         # the Y key again; E: done
finish_probe
check "ActivateKeyboardLayout(German) in the foreground: the thread has German" "|ACT 04090409 04070409|"
expect "...and the keyboard types German (the X keyboard's group)" "$g1" 1
check "...the Y key is VK_Z z, with no input language request" "|ACT 04090409 04070409|DOWN 5a 15|CHAR 007a|BG"
check "a background thread's ActivateKeyboardLayout(US): its own layout" "|BG 04070409 04090409|"
expect "...and the keyboard still types German" "$g2" 1
check "...the Y key is still VK_Z z" "|BG 04070409 04090409|DOWN 5a 15|CHAR 007a|"
$K $SWITCH s300; [ "$(group)" = 0 ] || $K $SWITCH s300   # back to US for part 2

# --- 2. a compositor owns the keyboard ---
echo "2. a compositor (SG_LOCK_CONTROL)"
python3 - "$T/ctl.sock" "$T/asked" <<'EOS' & SP=$!
import os, socket, sys
path, log = sys.argv[1], sys.argv[2]
s = socket.socket(socket.AF_UNIX); s.bind(path); s.listen(8)
while True:
    c, _ = s.accept()
    line = c.recv(256).decode().strip()
    with open(log, "a") as f: f.write(line + "\n")
    c.sendall(b"OK\n"); c.close()
EOS
i=0; while [ ! -S "$T/ctl.sock" ] && [ $i -lt 40 ]; do sleep 0.1; i=$((i + 1)); done
export SG_LOCK_CONTROL="$T/ctl.sock"
: > "$T/asked"
start_probe
$K +24 -24 s800                      # Q: the foreground thread activates German
g3=$(group)
asked1=$(tr '\n' '|' < "$T/asked")
$K +25 -25 s100 $SWITCH $SWITCH s2500   # W: the program is busy; the person switches twice meanwhile
$K +26 -26 s400                      # E: done
finish_probe
asked2=$(tr '\n' '|' < "$T/asked")
expect "ActivateKeyboardLayout(German) in the foreground asks the compositor for layout 1" "$asked1" "LAYOUT 1|"
expect "...and leaves the X server's group to it" "$g3" 0
check "the person's two switches while the program was busy: two requests" "|STALL|REQ"
expect "...which are not sent back to the compositor as the program's" "$asked2" "LAYOUT 1|"
unset SG_LOCK_CONTROL

[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
