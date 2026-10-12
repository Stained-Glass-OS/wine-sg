#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Recent documents from the open dialog (patches/sg/2056). The file picked in
# an open dialog goes in the recent documents (a shortcut in the Recent
# folder) unless the program asked for OFN_DONTADDTORECENT; Cancel adds
# nothing; every file of a multiple selection is added. A save dialog with
# OFN_NOREADONLYRETURN refuses a read-only file with a message (and stays),
# without the flag takes it.
#
#   WINE=/opt/wine-sg/bin/wine test/recentdocs-gate.sh
#   Mutants (comdlg32 itemdlg.c): SG_MUTANT_NO_RECENT: the plain open and the
#   multiple selection fail; READONLY_RETURNED: the read-only file is returned.
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
T=$(mktemp -d /var/tmp/sg-recentdocs.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/recentdocs-probe.c" -lcomdlg32 -lshell32 -lole32 || { fail "the probe did not build"; exit 1; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run"
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
run() { timeout 60 "$WINE" "$T/probe.exe" "$1" 2>/dev/null | tr -d '\r' | tail -1; }
for c in "default 1" "norecent 0" "cancel 0" "multi 2"; do
    set -- $c
    r=$(run "$1")
    [ "$r" = "recent=$2" ] && pass "$1: $r" || fail "$1: '$r' (want recent=$2)"
done
for c in "readonly message=1_result=cancelled" "readonlyok message=0_result=recentprobe-ro.txt"; do
    set -- $c
    r=$(run "$1")
    want=$(echo "$2" | tr _ ' ')
    [ "$r" = "$want" ] && pass "$1: $r" || fail "$1: '$r' (want $want)"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
