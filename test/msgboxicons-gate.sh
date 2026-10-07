#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The message box icons are ours (theme/icons.py; patches/sg/1351). Wine's
# own painted error, warning, information and question icons showed in every
# message box, task dialog and shell stock icon. Ours are flat Windows 10
# shapes, drawn for every size from 16 to 256 px: a red disc with a white
# cross, an amber triangle with a dark "!", blue discs with a white "i" and
# "?". shell32's SHGetStockIconInfo gives the same drawings for SIID_ERROR,
# SIID_WARNING, SIID_INFO and SIID_HELP (it gave the plain file icon).
#
# test/msgboxicons-probe.c checks, for each of the four:
#   * user32's icon group has frames 16 20 24 32 40 48 64 96 128 256;
#   * none of them is one of Wine's original frames (their FNV-1a hashes
#     are below);
#   * drawn at 32, 48 and 256 px on white and on near-black, its body and
#     glyph are the colours designed (64- and 32-bit);
#   * SHGetStockIconInfo names shell32's own icon and draws the same;
#   * a MessageBox with MB_ICON* and a task dialog with TD_*_ICON show them
#     (with a display: Xvfb).
#
#   WINE=/opt/wine-sg/bin/wine test/msgboxicons-gate.sh
# Mutants: SG_MUTANT_MSGBOX_ICONS=1 when theme/icons.py runs (the error and
# information discs swap colours); -DSG_MUTANT_STOCKICONS in
# dlls/shell32/iconcache.c; and the series without 1351 (Wine's art).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

# Wine 10.0's own frames of oic_hand, oic_bang, oic_note and oic_ques.ico
OLD="8435cee11af479df cec5d24970387170 1303f9a9ac97c2f6 0b8aa065a001479a 2a0836670d7cd566 52355e53cd24e798
4d398b1deb8dd79f 773f425f71b1cacb 0d02737c3fe67a8d fe6d803409085235 72f229105578cbee 125627a78230df4b
9af4ef2daf824a78 bc03adf0cb4f500b 96ec586e5f326773 8936c25a756766c8 a0c87672833eb946 0bf16248ae0f8db9
b53287db81acd3ce c5fb82857c603d57 96b5946cb5799b4e ff42c8c562b69202 a788cb0e0d49ba0f 0ff98ec814932c6c
8826f10c3aa9f091 10e6ec8e1379336e 95d0f2649f6f2317 22f6e32763e67687 781e70576d19d459 6187257bd7c5de33
6220221d1e866732 1aa6a2b427ccdb51 288e15c4db9ddf03 6b75c6a9a3a55a0a deb0f9b6059bd714 e9e0339ada3a2835
b1e4e7b240036133 c5129cb3df7a3529 519ea8ef64c33536 8c098b20e528584f"
SIZES="16 20 24 32 40 48 64 96 128 256"

T=$(mktemp -d /var/tmp/sg-msgboxicons.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for m in "$MINGW:64" "$MINGW32:32"; do
    TMPDIR=/var/tmp "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/msgboxicons-probe.c" -luser32 -lgdi32 ||
        { fail "probe did not build"; exit 1; }
done
GUI=""
if command -v Xvfb >/dev/null; then
    DPY=$((300 + $$ % 400))
    while [ -e "/tmp/.X$DPY-lock" ]; do DPY=$((DPY + 1)); done
    Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    export DISPLAY=":$DPY"
    GUI=gui
else
    echo "NOTE: no Xvfb: the message box and task dialog are not checked"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# class R,G,B: red, amber, blue, white, dark or other
class() {
    echo "$1" | awk -F, '{ r = $1; g = $2; b = $3
        if (r >= 230 && g >= 230 && b >= 230) print "white"
        else if (r <= 70 && g <= 70 && b <= 70) print "dark"
        else if (r >= 200 && g >= 150 && b <= 110) print "amber"
        else if (r >= 170 && g <= 110 && b <= 110) print "red"
        else if (b >= 170 && r <= 110) print "blue"
        else print "other" }'
}
want_body() { case $1 in error) echo red ;; warning) echo amber ;; *) echo blue ;; esac; }
want_glyph() { case $1 in warning) echo dark ;; *) echo white ;; esac; }
stock_id() { case $1 in error) echo -1350 ;; warning) echo -1351 ;; information) echo -1352 ;; question) echo -1353 ;; esac; }

for b in 64 32; do
    if [ $b = 64 ]; then arg=$GUI; else arg=""; fi
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe$b.exe" $arg 2>/dev/null | tr -d '\r')
    [ -n "$out" ] || { fail "$b-bit: the probe printed nothing"; continue; }
    for n in error warning information question; do
        fr=$(printf '%s\n' "$out" | sed -n "s/^frames $n //p")
        [ "$fr" = "$SIZES" ] && pass "$b-bit $n: frames $fr" || fail "$b-bit $n: frames '$fr' (want $SIZES)"
        old=0
        for h in $(printf '%s\n' "$out" | sed -n "s/^hash $n //p"); do
            case " $(echo $OLD) " in *" $h "*) old=$((old + 1)) ;; esac
        done
        [ $old = 0 ] && pass "$b-bit $n: none of Wine's own frames is left" || fail "$b-bit $n: $old of Wine's own frames still there"
        st=$(printf '%s\n' "$out" | sed -n "s/^stock $n iIcon=//p")
        [ "$st" = "$(stock_id $n)" ] && pass "$b-bit $n: SHGetStockIconInfo gives shell32's icon $st" ||
            fail "$b-bit $n: SHGetStockIconInfo gives '$st' (want $(stock_id $n))"
        wheres="user32 stock"
        [ -n "$arg" ] && wheres="$wheres msgbox"
        [ -n "$arg" ] && [ $n != question ] && wheres="$wheres taskdlg"
        for w in $wheres; do
            lines=$(printf '%s\n' "$out" | grep "^colour $w $n ")
            [ -n "$lines" ] || { fail "$b-bit $n ($w): not drawn"; continue; }
            bad=$(printf '%s\n' "$lines" | while read -r _ _ _ size bg body glyph; do
                case "$size" in none) echo "no icon"; continue ;; esac
                bc=$(class "${body#body=}"); gc=$(class "${glyph#glyph=}")
                [ "$bc" = "$(want_body $n)" ] && [ "$gc" = "$(want_glyph $n)" ] ||
                    echo "${size}px on $bg: body ${body#body=} ($bc), glyph ${glyph#glyph=} ($gc)"
            done)
            [ -z "$bad" ] && pass "$b-bit $n ($w): $(want_body $n) body, $(want_glyph $n) glyph, on light and dark" ||
                fail "$b-bit $n ($w): $bad"
        done
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
