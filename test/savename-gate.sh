#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Save As takes the name as typed (1129). GetSaveFileName goes through the
# item dialog since 0582, which (a) did not select the name it suggests, so a
# typed name went in front of it ("typed.rtfDocument.rtf"), and (b) put the
# chosen type's extension after any other ("pic.rtf" with Word Document
# chosen became "pic.rtf.docx"). WordPad's Save As broke both ways
# (sg-shell wordpad-check: 21 failures). With "Document" suggested and Word
# Document (*.docx) chosen:
#   1. the suggested name is selected when the dialog opens
#   2. "pic.rtf" (another of its types) -> pic.rtf
#   3. "notes.txt" -> notes.txt; "page.html" (registered) -> page.html
#   4. "report" -> report.docx; "data.zz9" (unknown) -> data.zz9.docx
#
#   WINE=/opt/wine-sg/bin/wine test/savename-gate.sh
#   Mutants (comdlg32 itemdlg.c): SG_MUTANT_NAME_UNSELECTED: 1 fails;
#   SG_MUTANT_TYPED_EXT_REPLACED: 2 and 3 fail.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-savename.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/savename-probe.c" -lcomdlg32 || { fail "the probe did not build"; exit 1; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run"
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
run() { timeout 60 "$WINE" "$T/probe.exe" "$1" 2>/dev/null | tr -d '\r' | tail -1; }
r=$(run sel)
[ "$r" = "sel=0-8 len=8" ] && pass "the suggested name is selected ($r)" || fail "the suggested name: '$r' (want sel=0-8 len=8: "Document", all selected)"
for c in "pic.rtf pic.rtf" "notes.txt notes.txt" "page.html page.html" "report report.docx" "data.zz9 data.zz9.docx"; do
    set -- $c
    r=$(run "$1")
    [ "$r" = "file=$2" ] && pass "typed '$1' with Word Document chosen: $2" || fail "typed '$1': '$r' (want file=$2)"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
