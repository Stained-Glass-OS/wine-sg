#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A windowless RichEdit drawn into a view of a new size forgets a scroll position its text no longer
# needs (patches/sg/7001). Zoom's Join dialog sets the "Your name" box's text while the box has no
# width yet -- the caret at the end scrolls the whole text out of the view -- and then draws it with
# ITextServices::TxDraw into the real box: only the name's last letter showed, at the box's left
# edge. TxDraw now updates the scroll range after laying the text out again, as a change of the
# host's client rectangle already did. The probe (test/txdrawscroll-probe.cpp) is such a host,
# 64- and 32-bit (a thiscall host, as Windows calls it).
#   WINE=/opt/wine-sg/bin/wine test/txdrawscroll-gate.sh
# Mutant: SG_MUTANT_TXDRAW_KEEPS_SCROLL (dlls/riched20/txtsrv.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-g++ i686-w64-mingw32-g++; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-txdrawscroll.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cxx=x86_64-w64-mingw32-g++; [ $b = 32 ] && cxx=i686-w64-mingw32-g++
    TMPDIR=/var/tmp $cxx -O1 -static -o "$T/probe$b.exe" "$HERE/txdrawscroll-probe.cpp" -lgdi32 -luuid \
        || { fail "probe ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    out=$(timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$b-bit: $2"
        else fail "$b-bit: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check created                "riched20 creates text services for the host"
    check drawn                  "the text set while the view had no width is drawn in the wider view"
    check starts_at_left         "it starts at the view's left edge, not scrolled out of it"
    check scroll_reset           "the horizontal scroll position is back at 0"
    check same_again             "a second draw at the same size draws the same"
    check narrow_still_scrolled  "text that does not fit keeps the caret's scroll position"
    check done                   "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
