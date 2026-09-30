#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The common file dialogs look like the rest of the system (patches/sg/0582).
# A GetOpenFileName or GetSaveFileName with no hook and no template shows the
# item dialog -- address box, folder tree -- as Windows does since Vista,
# instead of the older "Look in" dialog (David: the common dialogs looked
# old). The probe opens one of each (the Unicode open, the ANSI save), types a
# name and presses Open/Save: the item dialog must have been shown, its address
# box must name the starting folder, and the result must come back in each
# form's own layout (path, offsets, filter index, default extension).
#
#   WINE=/opt/wine-sg/bin/wine test/ofn-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ofn.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/ofn-probe.exe" "$HERE/ofn-probe.c" -lcomdlg32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/ofn-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout 120 "$WINE" 'C:\ofn-probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
o=$(printf '%s\n' "$out" | grep '^ofn='); s=$(printf '%s\n' "$out" | grep '^sfn=')
case "$o" in *"itemdlg=yes"*) pass "GetOpenFileName with no hook shows the item dialog" ;; *) fail "GetOpenFileName: $o" ;; esac
case "$o" in *'address=C:\ '*) pass "its address box names the starting folder" ;; *) fail "address box: $o" ;; esac
[ "$o" = 'ofn=1 itemdlg=yes address=C:\ file=C:\ofn-test.txt offset=3 ext=12 filter=1' ] \
    && pass "the Unicode open returns the path, offsets and filter" || fail "Unicode open result: $o"
[ "$s" = 'sfn=1 itemdlg=yes file=C:\saved.txt offset=3 ext=9' ] \
    && pass "the ANSI save returns the path with the default extension" || fail "ANSI save result: $s"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
