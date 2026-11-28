#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Common controls driven as a Walk program drives them (patches/sg/8420; i1013, WireGuard's
# window: no Remove/Export buttons, a black strip beside the tabs, a grey tunnel list).
#  1. a wrapping list-style toolbar (TBSTYLE_WRAPABLE|TBSTYLE_LIST, autosized buttons, one with
#     text) whose WM_WINDOWPOSCHANGED is answered by the program (Walk sends TB_AUTOSIZE and
#     passes nothing on, so no WM_SIZE): TB_GETIDEALSIZE is the one-row width whatever the
#     present width, and at that width every button is on one row
#  2. a tab control printed (WM_PRINTCLIENT, PRF_ERASEBKGND) into a black bitmap, as Walk's
#     TabWidget paints it: the strip beside the tabs is erased, not left black
#  3. a double-buffered list view whose WM_ERASEBKGND the program answers: a repaint, and a
#     print, show the list's own background, not what was behind it
# The probe is test/walkui-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/walkui-gate.sh
# Mutants (dlls/comctl32): SG_MUTANT_WRAP_CACHED_WIDTH, SG_MUTANT_WRAP_FIXED_WIDTH,
#   SG_MUTANT_IDEAL_WRAPPED (toolbar.c), SG_MUTANT_TAB_PRINT_NO_ERASE (tab.c),
#   SG_MUTANT_LV_DOUBLEBUFFER_NO_FILL (listview.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-walkui.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/walkui-probe.c" -lcomctl32 -lgdi32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p" | head -1; }

set -- $(v TB_IDEAL)
if [ -n "${1:-}" ] && [ "$1" = "${2:-}" ] && [ "$1" = "${4:-}" ]; then
    pass "toolbar: the ideal width is the one-row width ($1), laid out narrow or wide"
else
    fail "toolbar: ideal width narrow ${1:-?}, wide ${2:-?}, buttons end at ${4:-?}"
fi
[ "$(v TB_ROWS)" = 1 ] && pass "toolbar: at its ideal width every button is on one row (no WM_SIZE needed)" \
    || fail "toolbar: $(v TB_ROWS) rows at its ideal width"
case "$(v TAB_STRIP)" in ''|000000) fail "tab control: the strip beside the tabs stayed black when printed ($(v TAB_STRIP))" ;;
    *) pass "tab control: printed with PRF_ERASEBKGND, the strip beside the tabs is erased" ;; esac
set -- $(v LV_PAINT)
[ -n "${1:-}" ] && [ "$1" = "${3:-}" ] && pass "list view: a double-buffered repaint shows its own background" \
    || fail "list view: repaint shows ${1:-?}, want ${3:-?}"
set -- $(v LV_PRINT)
[ -n "${1:-}" ] && [ "$1" = "${3:-}" ] && pass "list view: printed, its background is its own" \
    || fail "list view: print shows ${1:-?}, want ${3:-?}"
exit $RC
