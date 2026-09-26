#!/bin/sh
# The X keyboard layout's keys in Wine (patches/sg/0250, 0251), under Xvfb,
# with setxkbmap:
#
#  - German: ß, ´, ü, ö, ä have their scan codes (0x0c, 0x0d, 0x1a, 0x27,
#    0x28) and say so through MapVirtualKeyEx/ToUnicodeEx -- in a UTF-8
#    locale they had none (0250); z at 0x15 and y at 0x2c (QWERTZ);
#  - Ctrl+Alt is AltGr in ToUnicodeEx: German @ on Q, | on <; French # on 3,
#    é unshifted on 2; the US layout's Ctrl+Alt+Q still gives nothing (0251);
#  - a program started on the US layout follows a switch to German: its
#    labels are German and keys it sends by scan code type z, ü and
#    (Ctrl+Alt) @ -- Wine kept the old layout's tables when no MappingNotify
#    reached it (0250).
#
#   WINE=/opt/wine-sg/bin/wine test/kbdlayout-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${KBDLAYOUT_DPY:-175}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb setxkbmap xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-kbdlayout.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER LANG=C.UTF-8
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
"$MINGW" -municode -O2 -o "$C/probe.exe" "$HERE/kbdlayout-probe.c" -luser32 || { fail "probe did not build"; exit 1; }

labels() { setxkbmap -layout "$1"; "$WINE" 'C:\probe.exe' labels "C:\\$1.txt" >/dev/null 2>&1; tr -d '\r' < "$C/$1.txt"; }
# the line for scan code $2 in the labels of layout $1: vk plain shift altgr
line() { sed -n "s/^sc $2 vk //p" "$C/$1.txt" | tr -d '\r'; }

labels us > /dev/null
labels de > /dev/null
labels fr > /dev/null
[ "$(line de 0c | cut -d' ' -f2)" = 00df ] && [ "$(line de 1a | cut -d' ' -f2-3)" = "00fc 00dc" ] && \
    [ "$(line de 27 | cut -d' ' -f2)" = 00f6 ] && [ "$(line de 28 | cut -d' ' -f2)" = 00e4 ] \
    && pass "German: ß, ü/Ü, ö, ä at their scan codes" || fail "German: 0c=$(line de 0c) 1a=$(line de 1a) 27=$(line de 27) 28=$(line de 28)"
[ "$(line de 0d | cut -d' ' -f1)" != 00 ] && [ "$(line de 0d | cut -d' ' -f2)" = 00b4 ] \
    && pass "German: the dead acute key has its scan code ($(line de 0d))" || fail "German 0d: $(line de 0d)"
[ "$(line de 15 | cut -d' ' -f2)" = 007a ] && [ "$(line de 2c | cut -d' ' -f2)" = 0079 ] && pass "German: QWERTZ (z at 0x15, y at 0x2c)" \
    || fail "German: 15=$(line de 15) 2c=$(line de 2c)"
[ "$(line de 10 | cut -d' ' -f4)" = 0040 ] && [ "$(line de 56 | cut -d' ' -f4)" = 007c ] && pass "German Ctrl+Alt (AltGr): @ on Q, | on <" \
    || fail "German AltGr: 10=$(line de 10) 56=$(line de 56)"
[ "$(line fr 04 | cut -d' ' -f4)" = 0023 ] && [ "$(line fr 03 | cut -d' ' -f2)" = 00e9 ] && [ "$(line fr 10 | cut -d' ' -f2)" = 0061 ] \
    && pass "French: a on Q, é on 2, AltGr+3 is #" || fail "French: 10=$(line fr 10) 03=$(line fr 03) 04=$(line fr 04)"
[ "$(line us 10 | cut -d' ' -f4)" = - ] && [ "$(line us 10 | cut -d' ' -f2)" = 0071 ] && pass "US: Ctrl+Alt+Q still gives nothing" \
    || fail "US: 10=$(line us 10)"

# a program started on the US layout follows a switch to German
setxkbmap -layout us
rm -f "$C/ready" "$C/go"
"$WINE" 'C:\probe.exe' follow 'C:\follow.txt' 'C:\ready' 'C:\go' >/dev/null 2>&1 &
i=0; while [ ! -f "$C/ready" ] && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
setxkbmap -layout de
sleep 2
touch "$C/go"
i=0; while ! grep -q '^text' "$C/follow.txt" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
F=$(sed -n 's/^sc 15 vk //p' "$C/follow.txt" | tr -d '\r')
[ "$(echo "$F" | cut -d' ' -f2)" = 007a ] && pass "a running program's tables follow the switch (0x15 is z)" || fail "running program: 15=$F"
TXT=$(sed -n 's/^text //p' "$C/follow.txt" | tr -d '\r')
[ "$TXT" = "007a 00fc 0040" ] && pass "keys it sends by scan code type z, ü and (Ctrl+Alt) @" || fail "typed: '$TXT'"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
