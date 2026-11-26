#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The screen DC's bitmap survives a program deleting it (patches/sg/9627).
#
# GetCurrentObject(OBJ_BITMAP) of a screen DC gave the display's own bitmap,
# shared by every display DC of the process, and DeleteObject took it: from
# then on GetDIBits on any screen DC's bitmap failed. TightVNC's desktop
# server does exactly that (Screen::getBMI: GetDIBits for the screen's format,
# then DeleteObject and DeleteDC), so it ended at its second look and no VNC
# viewer ever got a picture (ADR 0010 pattern A). On Windows it is a system
# object, which DeleteObject leaves alone. test/displaybitmap-probe.c asks
# twice and then BitBlts from the screen.
#   WINE=/opt/wine-sg/bin/wine test/displaybitmap-gate.sh
# Mutant: SG_MUTANT_DISPLAY_BITMAP_DELETABLE (dlls/win32u/sysparams.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-dispbmp.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/displaybitmap-probe.exe" "$HERE/displaybitmap-probe.c" -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/displaybitmap-probe.exe" 2>/dev/null | tr -d '\r')
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
r=$(v FIRST)
case "$r" in 0*|"") fail "the first GetDIBits of the screen DC's bitmap failed ($r)";; *) pass "the screen DC's bitmap gives its format ($r)";; esac
r=$(v SECOND)
case "$r" in 0*|"") fail "after DeleteObject of it, the screen DC's bitmap is gone ($r)";;
    *) pass "a program deleting it changes nothing: asked again ($r)";; esac
[ "$(v BLT)" = 1 ] && pass "and the screen DC still copies" || fail "BitBlt from the screen after that: $(v BLT)"
echo
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
