#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The session's keyboard is its default layout (patches/sg/9901). Windows' default input layout is the
# one the person types with; the layout half of the default HKL was the user locale's whatever the X
# keyboard map, and the locale's HKL was answered from the X map, so on a German map in an English (US)
# session GetKeyboardLayout said US (0x04090409), and a program that loaded or activated US English got
# German answers. Now (test/kbddevice-probe.c; scan code 0x15 is Y on US, Z on German):
#   en-US, US map: 0x04090409 "00000409", as before
#   en-US, German map: 0x04070409 "00000407", 0x15 is Z; US English (00000409) is 0x04090409, 0x15 Y
#   en-US, US Dvorak map: 0xf0020409 "00010409", 0x10 is VK_OEM_7; US English 0x10 is Q
#   en-US, de(nodeadkeys): German, 0x04070409; an XKB layout no Windows layout has (epo): the locale's
#   de-DE, US map: 0x04090407 "00000409", 0x15 Y; German (00000407) is 0x04070407, 0x15 Z
#   WINE=/opt/wine-sg/bin/wine test/kbddevice-gate.sh   (KBDDEVICE_DPY=N: Xvfb :N, default 362)
# Mutant: SG_MUTANT_KBD_LOCALE_DEVICE (win32u/input.c, winex11.drv/xkbtables.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDDEVICE_DPY:-362}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo setxkbmap "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/de ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbddevice.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/kbddevice-probe.c" -luser32 \
    || { echo "FAIL  the probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
# run LANG XKB-LAYOUT XKB-VARIANT KLID: the probe's two lines, joined
run() {
    setxkbmap -display ":$DPY" -layout "$2" ${3:+-variant "$3"} 2>/dev/null
    LANG=$1 LC_ALL=$1 timeout -s KILL 60 "$WINE" "$T/probe.exe" "$4" 2>/dev/null </dev/null | tr -d '\r' | tr '\n' ' '
}
check() {  # check WHAT GOT WANT-SUBSTRING...
    what=$1 got=$2; shift 2
    for w in "$@"; do case "$got" in *"$w"*) ;; *) echo "FAIL  $what: got '$got', want '$w'"; rc=1; return;; esac; done
    echo "PASS  $what"
}
o=$(run en_US.UTF-8 us "" 00000407)
check "en-US, US map: 0x04090409 00000409" "$o" "HKL 04090409 NAME 00000409 SC10 51 SC15 59" "LOADED 04070407 SC10 51 SC15 5a"
o=$(run en_US.UTF-8 de "" 00000409)
check "en-US, German map: 0x04070409 00000407, 0x15 is Z" "$o" "HKL 04070409 NAME 00000407 SC10 51 SC15 5a"
check "...US English loaded there is 0x04090409 with 0x15 Y" "$o" "LOADED 04090409 SC10 51 SC15 59"
o=$(run en_US.UTF-8 us dvorak 00000409)
check "en-US, US Dvorak map: 0xf0020409 00010409, 0x10 is VK_OEM_7" "$o" "HKL f0020409 NAME 00010409 SC10 de"
check "...US English loaded there has 0x10 Q" "$o" "LOADED 04090409 SC10 51"
o=$(run en_US.UTF-8 de nodeadkeys "")
check "en-US, de(nodeadkeys): German" "$o" "HKL 04070409 NAME 00000407"
o=$(run en_US.UTF-8 epo "" "")
check "en-US, an XKB layout no Windows layout has (epo): the locale's" "$o" "HKL 04090409 NAME 00000409"
o=$(run de_DE.UTF-8 us "" 00000407)
check "de-DE, US map: 0x04090407 00000409, 0x15 Y" "$o" "HKL 04090407 NAME 00000409 SC10 51 SC15 59"
check "...German loaded there is 0x04070407 with 0x15 Z" "$o" "LOADED 04070407 SC10 51 SC15 5a"
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
