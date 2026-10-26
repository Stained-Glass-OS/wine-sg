#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A toolbar button read with TB_GETBUTTON, deleted and inserted again with TB_INSERTBUTTONA keeps
# its text (patches/sg/7007). MFC's CToolBar changes a button that way, passing back the toolbar's
# own copy of the text; the copy was freed with the button and then read as ANSI, so WinMerge's
# menu bar (an MFC toolbar) showed no names. Deleted buttons' texts are kept until the toolbar is
# destroyed and the toolbar's own text pointers are taken as Unicode. The probe is
# test/tbreinsert-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/tbreinsert-gate.sh
# Mutant: SG_MUTANT_TB_RETIRED_STRING (dlls/comctl32/toolbar.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-regexpand.XXXXXX)
XPID=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XPID" ] && kill $XPID 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/probe$b.exe" "$HERE/tbreinsert-probe.c" -lcomctl32 -luser32 || { fail "probe ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset WAYLAND_DISPLAY
D=${SG_GATE_DISPLAY:-:286}
Xvfb "$D" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XPID=$!
export DISPLAY="$D"
sleep 1
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
    check added_texts        "texts added from buffers the program then frees"
    check reinserted_texts   "read, deleted and inserted again with TB_INSERTBUTTONA: the texts stay"
    check state_changed      "the change the program made is there"
    check reinserted_again   "the same twice more"
    check ansi_text          "a program's own ANSI text is still read as ANSI"
    check done                 "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
