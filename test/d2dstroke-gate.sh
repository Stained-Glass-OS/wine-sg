#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D stroke styles (patches/sg/9351), under Xvfb (test/d2dstroke-probe.cpp): dashes (built-in and
# custom, with the dash offset), dash caps on dots, round and square start/end caps, bevel and round joins,
# DrawRectangle with a style, a fixed stroke under a scale. Before, every style was drawn as a plain solid
# line with flat caps and mitred corners ("Ignoring stroke style"); Paint.NET 5.2 draws through 18 of them.
#   WINE=/opt/wine-sg/bin/wine test/d2dstroke-gate.sh   (WINESERVER=... beside a build tree)
# Mutants: SG_MUTANT_D2D_STROKE_STYLE_IGNORED (dlls/d2d1/device.c), SG_MUTANT_D2D_NO_DASHES
# (dlls/d2d1/geometry_combine.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb xdpyinfo "$MINGWXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dstroke.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/d2dstroke-probe.exe" "$HERE/d2dstroke-probe.cpp" -ld2d1 -ld3d11 -ldxgi \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
DPY="${D2DSTROKE_DPY:-383}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
unset WAYLAND_DISPLAY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dstroke-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct2D device here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check solid_plain          "no style: a solid line, as before"
check dash_first           "DASH: the first dash"
check dash_gap             "DASH: the gap 2 stroke widths on"
check dash_second          "DASH: the second dash"
check dash_gap2            "DASH: the second gap"
check offset_gap           "dash offset 1: a gap where the first dash was"
check offset_dash          "dash offset 1: the next dash 4 px earlier"
check custom_dash          "custom dashes {3,1}: a dash"
check custom_gap           "custom dashes: the gap"
check custom_next          "custom dashes: the next dash"
check dot_round            "DOT with round dash caps: a dot"
check dot_between          "DOT: paper between dots"
check round_cap            "round start cap: ink before the start"
check round_cap_corner     "round cap: not a square"
check round_cap_end        "round end cap"
check square_cap_corner    "square cap: its corner"
check square_cap_beyond    "square cap: half a width only"
check flat_cap             "no style: flat ends, as before"
check join_miter_default   "no style: a mitred corner, as before"
check join_bevel           "bevel join: the corner cut"
check join_bevel_inside    "bevel join: the stroke below the cut"
check join_round_in        "round join: inside its radius"
check join_round_out       "round join: outside its radius"
check rect_dash            "DrawRectangle with DASH: a dash"
check rect_gap             "DrawRectangle with DASH: a gap"
check fixed_on             "fixed stroke under a 4x scale: the line"
check fixed_thin           "fixed stroke: 2 px, not 8"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
