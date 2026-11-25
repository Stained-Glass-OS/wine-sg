#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The X keyboard map's layouts are its group names (patches/sg/9153). Under Xwayland the X map is the
# compositor's keymap, compiled from its text: _XKB_RULES_NAMES keeps the X server's defaults ("evdev",
# "pc105", "us") and the symbols are "(unnamed)", so winex11, which read the rules names, took every
# desktop's keyboard for US English: on a German desktop the default HKL was 0x04090409 "00000409", and
# US English loaded there answered German. The gate gives an Xvfb the keymap the way Xwayland is given it
# (test/kbdxtool.c: xkbcommon's keymap text, as sg-compositor sends it; loaded with xkbcomp, which leaves
# the rules names alone), then (test/kbddevice-probe.c, test/kbdvkeys-probe.c; scan 0x15 is Y on US):
#   German: 0x04070409 "00000407", 0x15 Z; US English loaded there 0x04090409, 0x15 Y
#   French: 0x040c0409 "0000040c", 0x10 A; US Dvorak: 0xf0020409 "00010409"; US: 0x04090409
#   Swedish: 0x041d0409, its O-umlaut key VK_OEM_3 (9152)
#   WINE=/opt/wine-sg/bin/wine test/kbdxwayland-gate.sh   (KBDXWL_DPY=N: Xvfb :N, default 471)
# Mutant: SG_MUTANT_KBD_RULES_NAMES (winex11.drv/xkbtables.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDXWL_DPY:-471}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo xkbcomp xprop cc "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/de ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
[ -f /usr/include/xkbcommon/xkbcommon.h ] && [ -f /usr/include/X11/extensions/XTest.h ] || { echo "SKIP: no libxkbcommon-dev/libxtst-dev"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdxwl.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
cc -O2 -o "$T/kbdxtool" "$HERE/kbdxtool.c" -lxkbcommon -lXtst -lX11 || { echo "FAIL  kbdxtool did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/device.exe" "$HERE/kbddevice-probe.c" -luser32 \
    && TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/vkeys.exe" "$HERE/kbdvkeys-probe.c" -luser32 \
    || { echo "FAIL  the probes did not build"; exit 1; }
# -noreset: an X server resets (and drops the keyboard map it was given) when its last client leaves
Xvfb ":$DPY" -screen 0 800x600x24 -nolisten tcp -noreset >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
# keymap LAYOUT VARIANT: the compositor's keymap for LAYOUT(VARIANT), as Xwayland loads it
keymap() {
    "$T/kbdxtool" keymap "$1" "$2" "" > "$T/keymap.xkb" && xkbcomp -w0 "$T/keymap.xkb" "$DISPLAY" 2>/dev/null
}
run() { exe=$1; shift; timeout -s KILL 60 "$WINE" "$T/$exe" "$@" 2>/dev/null </dev/null | tr -d '\r' | tr '\n' ' '; }
check() {  # check WHAT GOT WANT-SUBSTRING...
    what=$1 got=$2; shift 2
    for w in "$@"; do case "$got" in *"$w"*) ;; *) echo "FAIL  $what: got '$got', want '$w'"; rc=1; return;; esac; done
    echo "PASS  $what"
}
keymap de ""
case "$(xprop -root _XKB_RULES_NAMES)" in
    *'"evdev", "pc105", "us"'*) echo "PASS  the precondition: the keymap is German, the rules names still say us (as on Xwayland)" ;;
    *) echo "FAIL  the precondition: the rules names changed ($(xprop -root _XKB_RULES_NAMES))"; rc=1 ;;
esac
"$T/kbdxtool" group | grep -q "name 0 German" || { echo "FAIL  the precondition: the keymap's group is not German"; rc=1; }
o=$(run device.exe 00000409)
check "German keymap: 0x04070409 00000407, 0x15 is Z" "$o" "HKL 04070409 NAME 00000407 SC10 51 SC15 5a"
check "...US English loaded there is 0x04090409 with 0x15 Y" "$o" "LOADED 04090409 SC10 51 SC15 59"
keymap fr ""
check "French keymap: 0x040c0409 0000040c, 0x10 is A" "$(run device.exe "")" "HKL 040c0409 NAME 0000040c SC10 41"
keymap us dvorak
check "US Dvorak keymap: 0xf0020409 00010409, 0x10 is VK_OEM_7" "$(run device.exe "")" "HKL f0020409 NAME 00010409 SC10 de"
keymap se ""
check "Swedish keymap: 0x041d0409, ö is VK_OEM_3" "$(run vkeys.exe - S27)" "HKL 041d0409 SC27=c0"
keymap us ""
check "US keymap: 0x04090409 00000409, 0x15 Y" "$(run device.exe "")" "HKL 04090409 NAME 00000409 SC10 51 SC15 59"
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
