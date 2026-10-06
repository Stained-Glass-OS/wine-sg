#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CPSUI as printer drivers' UI DLLs use it (patches/sg/1026): a property
# sheet built by a PFNPROPSHEETUI function that adds another (which gets a
# group handle of its own), pages, a group parent, a page inserted before
# another, a COMPROPSHEETUI on the treeview page and one on a page of its
# own whose controls are numbered from BegCtrlID, data blocks, CPSUI's
# strings, a page title changed; then the program works the sheet like a
# user (test/cpsui-probe.c, our own):
#   - the pages are there, in order, with their titles;
#   - the treeview lists the options with their values; choosing one shows
#     its control; a change reaches the OPTITEM and the caller's callback,
#     whose "options changed" hides an option;
#   - the page of its own has its combo box filled and selected, and a
#     choice reaches the OPTITEM;
#   - OK calls APPLYNOW, and the result reaches the PFNPROPSHEETUI functions
#     (SET_RESULT, also through a page's own handle), and the caller.
#   WINE=/opt/wine-sg/bin/wine test/cpsui-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-cpsui.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
trap '"$WINESERVER" -k 2>/dev/null; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
for i in $(seq 50); do [ -s "$T/display" ] && break; sleep 0.1; done
export DISPLAY=":$(cat "$T/display")"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
"$MINGW" -DUNICODE -D_UNICODE -municode -O1 -o "$T/cpsui.exe" "$HERE/cpsui-probe.c" -lcompstui -lcomctl32 ||
    { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >"$T/wineboot.log" 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/cpsui.exe" "$C/"
OUT=$( (cd "$C" && timeout 120 "$WINE" 'C:\cpsui.exe' </dev/null 2>>"$T/stderr.log") | tr -d '\r')
has() { printf '%s\n' "$OUT" | grep -qxF "$1"; }
check() { if has "$2"; then pass "$1"; else fail "$1 (wanted '$2')"; fi; }

check "an added PFNPROPSHEETUI function gets a group of its own" "child init lparam $(printf '%s\n' "$OUT" | sed -n 's/^child init lparam \([0-9]*\).*/\1/p') own group 1"
check "a COMPROPSHEETUI on the treeview page" "tree compropsheetui 1 pages 1"
check "a page inserted before another (CPSFUNC_INSERT_PSUIPAGE)" "inserted before 1"
check "a COMPROPSHEETUI on a page of its own" "own compropsheetui 1 pages 1"
check "a data block stored and read back" "datablock 5 hello"
check "the group's page count" "pagecount 4"
check "the group's page handles" "hpsuipages 4"
check "CPSUI's own strings" "cpsui string Portrait"
check "the pages in order, a title changed" "tabs 4 Before|Renamed|SG Printer|SG Custom|"
check "a page gets its PSPINFO" "plain page pspinfo 1 group 1"
check "the treeview lists the options" "tree items 6"
check "an option's value is in the tree" "tree text Colour: Red"
check "choosing an option shows its control" "change area combo entries 3 sel 0"
check "a change reaches the caller's callback" "callback sel_changed item 1 old 0 new 2"
check "the tree shows the new value" "tree text after Colour: Blue"
check "the callback's change (an option hidden) shows" "tree items after 5"
check "a page of its own: controls from BegCtrlID" "own page title Paper combo entries 2 sel 0"
check "a choice there reaches its callback" "own callback sel_changed new 1"
check "OK is APPLYNOW to the caller" "callback applynow userdata 4242"
check "a page's own handle stands for its group" "page set_result 2"
check "the result reaches the program" "sheet ret 1 result 1"
check "both PFNPROPSHEETUI functions saw each result" "applied 1 sel_changed 1 child_results 3 top_results 3"
check "the options hold what was chosen" "colour 2 copies 5 collate hidden 1 paper 1"

if [ $RC != 0 ]; then echo "--- the program said:"; printf '%s\n' "$OUT" | tail -40; echo "--- Wine's errors:"; grep -v "^$" "$T/stderr.log" 2>/dev/null | grep -iv "fixme" | tail -15; fi
[ $RC = 0 ] && echo "cpsui gate: PASS" || echo "cpsui gate: FAIL"
exit $RC
