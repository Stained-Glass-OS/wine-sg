#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The keys of a layout have its Windows layout's virtual keys (patches/sg/9152). Each Windows layout names
# the character keys of its main block its own way (kbdsw's O-umlaut key is VK_OEM_3, kbduk's # key
# VK_OEM_7, kbd106's @ key VK_OEM_3); Wine gave the US key at that place, for the X map's own layout
# (Spanish, UK, Norwegian, Swiss, Turkish, Belgian ...) and for any layout loaded beside it. Per layout
# family, MapVirtualKeyEx scan -> virtual key (and VkKeyScanEx) of the X map's layout (setxkbmap) and of
# layouts loaded on a US map, against the layouts' published tables (kbdlayout.info); French, German,
# Dvorak, Japanese, Russian, Korean and US were right before and must stay.
#   WINE=/opt/wine-sg/bin/wine test/kbdvkeys-gate.sh   (KBDVKEYS_DPY=N: Xvfb :N, default 470)
# Mutant: SG_MUTANT_KBD_US_OEM (winex11.drv/xkbtables.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDVKEYS_DPY:-470}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo setxkbmap "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/se ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdvkeys.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/kbdvkeys-probe.c" -luser32 \
    || { echo "FAIL  the probe did not build"; exit 1; }
# -noreset: an X server resets (and drops a keyboard map set with setxkbmap) when its last client leaves
Xvfb ":$DPY" -screen 0 800x600x24 -nolisten tcp -noreset >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
# x LAYOUT VARIANT KLID|- ARGS...: the probe's line with the X map LAYOUT(VARIANT)
x() {
    setxkbmap -display ":$DPY" -layout "$1" -variant "$2" -option "" 2>/dev/null
    shift 2
    timeout -s KILL 60 "$WINE" "$T/probe.exe" "$@" 2>/dev/null </dev/null | tr -d '\r'
}
check() {  # check WHAT GOT WANT-SUBSTRING...
    what=$1 got=$2; shift 2
    for w in "$@"; do case " $got " in *" $w "*) ;; *) echo "FAIL  $what: got '$got', want '$w'"; rc=1; return;; esac; done
    echo "PASS  $what"
}
# the X map's own layout (the session's keyboard)
check "X map UK (kbduk): ' VK_OEM_3, \` VK_OEM_8, # VK_OEM_7, \\ VK_OEM_5" "$(x gb "" - S28 S29 S2b S56)" \
    SC28=c0 SC29=df SC2b=de SC56=dc
check "X map Norwegian (kbdno): ø VK_OEM_3, + VK_OEM_PLUS, \\ VK_OEM_4, å VK_OEM_6, ¨ VK_OEM_1, | VK_OEM_5, ' VK_OEM_2, - VK_OEM_MINUS" \
    "$(x no "" - S27 S0c S0d S1a S1b S29 S2b S35)" SC27=c0 SC0c=bb SC0d=db SC1a=dd SC1b=ba SC29=dc SC2b=bf SC35=bd
check "X map Swedish (kbdsw): ö VK_OEM_3, ä VK_OEM_7, å VK_OEM_6" "$(x se "" - S27 S28 S1a)" SC27=c0 SC28=de SC1a=dd
check "X map Spanish (kbdsp): ñ VK_OEM_3, ' VK_OEM_4, ¡ VK_OEM_6, \` VK_OEM_1, + VK_OEM_PLUS" "$(x es "" - S27 S0c S0d S1a S1b)" \
    SC27=c0 SC0c=db SC0d=dd SC1a=ba SC1b=bb
check "X map Italian (kbdit): ò VK_OEM_3, ' VK_OEM_4, ì VK_OEM_6" "$(x it "" - S27 S0c S0d)" SC27=c0 SC0c=db SC0d=dd
check "X map Swiss German (kbdsg): ¨ VK_OEM_3, ö VK_OEM_7, ä VK_OEM_5, § VK_OEM_2, \$ VK_OEM_8" "$(x ch "" - S1b S27 S28 S29 S2b)" \
    SC1b=c0 SC27=de SC28=dc SC29=bf SC2b=df
check "X map Turkish Q (kbdtuq): * VK_OEM_8, , VK_OEM_COMMA, ö VK_OEM_2, ç VK_OEM_5, . VK_OEM_PERIOD" "$(x tr "" - S0c S2b S33 S34 S35)" \
    SC0c=df SC2b=bc SC33=bf SC34=dc SC35=be
check "X map Belgian (kbdbe): - VK_OEM_MINUS, = VK_OEM_PLUS" "$(x be "" - S0d S35)" SC0d=bd SC35=bb
check "X map Portuguese (kbdpo): + VK_OEM_PLUS, ´ VK_OEM_1" "$(x pt "" - S1a S1b)" SC1a=bb SC1b=ba
check "X map German (kbdgr): ^ VK_OEM_5, ß VK_OEM_4, Z" "$(x de "" - S29 S0c S15)" SC29=dc SC0c=db SC15=5a
check "X map French (kbdfr), as before: A, ; VK_OEM_PERIOD" "$(x fr "" - S10 S33)" SC10=41 SC33=be
check "X map Japanese (kbd106), as before: @ VK_OEM_3, : VK_OEM_1" "$(x jp "" - S1a S28)" SC1a=c0 SC28=ba
check "X map US Dvorak, as before: ' VK_OEM_7" "$(x us dvorak - S10)" SC10=de
check "X map US, as before: ; VK_OEM_1, ' VK_OEM_7, \` VK_OEM_3" "$(x us "" - S27 S28 S29)" SC27=ba SC28=de SC29=c0
# layouts loaded beside a US map
check "Swedish loaded on US: ö VK_OEM_3, + VK_OEM_PLUS; VkKeyScanEx ö, å" "$(x us "" 0000041d S27 S0c C00f6 C00e5)" \
    SC27=c0 SC0c=bb C00f6=00c0 C00e5=00dd
check "UK loaded on US: # VK_OEM_7, \\ VK_OEM_5; VkKeyScanEx #" "$(x us "" 00000809 S2b S56 C0023)" SC2b=de SC56=dc C0023=00de
check "Spanish loaded on US: ñ VK_OEM_3; VkKeyScanEx ñ" "$(x us "" 0000040a S27 C00f1)" SC27=c0 C00f1=00c0
check "Italian loaded on US: ò VK_OEM_3, ì VK_OEM_6" "$(x us "" 00000410 S27 S0d)" SC27=c0 SC0d=dd
check "Danish loaded on US: æ VK_OEM_3, ´ VK_OEM_4" "$(x us "" 00000406 S27 S0d)" SC27=c0 SC0d=db
check "Finnish loaded on US: ö VK_OEM_3, ä VK_OEM_7" "$(x us "" 0000040b S27 S28)" SC27=c0 SC28=de
check "Japanese loaded on US: @ VK_OEM_3, : VK_OEM_1, ; VK_OEM_PLUS, yen VK_OEM_5" "$(x us "" 00000411 S1a S28 S27 S7d)" \
    SC1a=c0 SC28=ba SC27=bb SC7d=dc
check "German loaded on US, as before: ^ VK_OEM_5, Z" "$(x us "" 00000407 S29 S15)" SC29=dc SC15=5a
check "French loaded on US, as before: A, , VK_OEM_COMMA" "$(x us "" 0000040c S10 S32)" SC10=41 SC32=bc
check "US Dvorak loaded on US, as before: ' VK_OEM_7" "$(x us "" 00010409 S10)" SC10=de
check "Russian loaded on US, as before: Q, ; VK_OEM_1" "$(x us "" 00000419 S10 S27)" SC10=51 SC27=ba
check "Korean loaded on US, as before: Q, ; VK_OEM_1" "$(x us "" 00000412 S10 S27)" SC10=51 SC27=ba
check "US-International loaded on US, as before: ' VK_OEM_7" "$(x us "" 00020409 S28)" SC28=de
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
