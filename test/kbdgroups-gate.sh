#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The session's keyboard layouts, and switching them is an input language change (patches/sg/9154, 9155).
# Setup lets a person pick two layouts; the session keymap has both as XKB groups, switched with the
# Windows logo key + Space. On Windows they are the loaded layouts (GetKeyboardLayoutList), the switch
# posts WM_INPUTLANGCHANGEREQUEST to the focus window and DefWindowProc activates the layout
# (WM_INPUTLANGCHANGE), and the keys are then the new layout's. Wine listed one layout, and a switch
# changed the characters only: with US and German, German Z came as VK_Y (Ctrl+Z was Ctrl+Y).
# ActivateKeyboardLayout(HKL_NEXT/HKL_PREV) failed (ERROR_CALL_NOT_IMPLEMENTED).
# An Xvfb is given the compositor's keymap "us,de" with grp:win_space_toggle the way Xwayland is
# (test/kbdxtool.c), real key events are sent with XTEST to test/kbdgroups-probe.c's window:
#   list 0x04090409 0x04070409; Y key: VK_Y y
#   Win+Space, Y key: REQUEST 0x04070409 (forward), WM_INPUTLANGCHANGE, VK_Z z; - key: VK_OEM_4 ß
#   Win+Space, Y key: REQUEST 0x04090409 (backward), VK_Y y
#   Win+Space, Q key: German; HKL_NEXT -> US, HKL_NEXT -> German, HKL_PREV -> US
#   WINE=/opt/wine-sg/bin/wine test/kbdgroups-gate.sh   (KBDGROUPS_DPY=N: Xvfb :N, default 472)
# Mutants: SG_MUTANT_KBD_ONE_GROUP (win32u/input.c, winex11.drv), SG_MUTANT_KBD_NO_NEXT (win32u/input.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDGROUPS_DPY:-472}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""; PP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo xkbcomp xdotool cc "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/de ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
[ -f /usr/include/xkbcommon/xkbcommon.h ] && [ -f /usr/include/X11/extensions/XTest.h ] || { echo "SKIP: no libxkbcommon-dev/libxtst-dev"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdgroups.XXXXXX)
cleanup() {
    [ -n "$PP" ] && kill "$PP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
cc -O2 -o "$T/kbdxtool" "$HERE/kbdxtool.c" -lxkbcommon -lXtst -lX11 || { echo "FAIL  kbdxtool did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/kbdgroups-probe.c" -luser32 || { echo "FAIL  the probe did not build"; exit 1; }
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
timeout -s KILL 90 "$WINE" "$T/probe.exe" > "$T/out" 2>/dev/null </dev/null & PP=$!
i=0; while ! grep -q '^READY' "$T/out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.25; i=$((i + 1)); done
WID=$(timeout 20 xdotool search --sync --name '^sgkbdgroups$' | head -n 1)
[ -n "$WID" ] && { xdotool windowactivate --sync "$WID" 2>/dev/null || xdotool windowfocus --sync "$WID"; }
sleep 1
K="$T/kbdxtool keys"
SWITCH="+133 s100 +65 s100 -65 s100 -133 s300"
$K +29 -29 s300                 # Y key (scan 0x15)
$K $SWITCH +29 -29 s300 +20 -20 s300   # to German: Y key, then the - key (scan 0x0c)
$K $SWITCH +29 -29 s300         # back to US: Y key
$K $SWITCH +24 -24 s300         # to German: Q key, then the probe's HKL_NEXT/HKL_PREV
i=0; while ! grep -q '^DONE\|^TIMEOUT' "$T/out" && [ $i -lt 80 ]; do sleep 0.25; i=$((i + 1)); done
tr -d '\r' < "$T/out" > "$T/log"
sed 's/^/      /' "$T/log"
# the lines that matter, in order: not the Windows logo key's and Space's own
grep -v '^DOWN 5b\|^DOWN 20\|^CHAR 0020\|^READY' "$T/log" | tr '\n' '|' > "$T/seq"
rc=0
check() {  # check WHAT PATTERN: PATTERN (lines joined by |) must be in the sequence
    case "$(cat "$T/seq")" in *"$2"*) echo "PASS  $1" ;; *) echo "FAIL  $1: want '$2'"; rc=1 ;; esac
}
check "GetKeyboardLayoutList: the session's two layouts, US then German" "LIST 2 04090409 04070409|"
check "US: the Y key is VK_Y, y" "|DOWN 59 15|CHAR 0079|"
check "Win+Space: WM_INPUTLANGCHANGEREQUEST German, forward; WM_INPUTLANGCHANGE; then the Y key is VK_Z, z" \
    "REQ 3 04070409|CHANGE 04070409|DOWN 5a 15|CHAR 007a|"
check "German: the - key is VK_OEM_4, ß" "|DOWN db 0c|CHAR 00df|"
check "Win+Space again: WM_INPUTLANGCHANGEREQUEST US, backward; the Y key is VK_Y again" \
    "REQ 5 04090409|CHANGE 04090409|DOWN 59 15|CHAR 0079|"
check "the layout after switching to German again" "|NOW 04070409|"
check "ActivateKeyboardLayout(HKL_NEXT): US (returns German)" "|NEXT 04070409 04090409|"
check "HKL_NEXT again: German, after the last the first" "|NEXT2 04090409 04070409|"
check "HKL_PREV: US" "|PREV 04070409 04090409|"
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
