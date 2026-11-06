#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bash from PowerShell in a Terminal tab (patches/sg/0790; David 2026-10-04:
# PowerShell opened from Start, "bash" did not work): with no Unix terminal
# under the console, bash.exe gives bash a terminal of its own (script(1)'s
# pseudo-terminal). On a pseudo console, as Terminal runs its shells: typed
# keys reach bash, its output comes back, the terminal's size is the
# console's and follows it, and bash's exit status is bash.exe's.
#
# And the console's scroll region (CSI r) and repeat (CSI b), which htop and
# vi use there.
#
#   WINE=/opt/wine-sg/bin/wine test/bashpty-gate.sh   (mutants SG_MUTANT_BASH_NO_BRIDGE, SG_MUTANT_VT_NO_REGION)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in script stty "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bashpty.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/bashpty-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/vtregion.exe" "$HERE/vtregion-probe.c" || { fail "vt probe did not build"; exit 1; }
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
D="$WINEPREFIX/drive_c"
printf 'PS1="sg$ "\n' > "$HOME/.bashrc"; printf '. ~/.bashrc\n' > "$HOME/.bash_profile"
timeout -s KILL 120 "$WINE" "$T/probe.exe" > "$T/out" 2>/dev/null
O=$(tr -d '\r' < "$D/bashpty-out.txt" 2>/dev/null | sed 's/\x1b\[[0-9;?]*[a-zA-Z]//g')
printf '%s\n' "$O" | grep -q 'SUM-42' && pass "typed keys reach bash and its output comes back (SUM-42)" \
    || fail "no SUM-42: $(printf '%s' "$O" | tail -c 300 | tr '\n' '|')"
printf '%s\n' "$O" | grep -q "^HERE-$HOME\$" && pass "started in the Windows profile, bash is in the home folder ($HOME)" \
    || fail "bash started in: $(printf '%s\n' "$O" | grep '^HERE-' | head -1)"
printf '%s\n' "$O" | grep -q '^30 100' && pass "the terminal is the console's size (30 100)" || fail "first size: $(printf '%s\n' "$O" | grep -E '^[0-9]+ [0-9]+' | head -2 | tr '\n' ' ')"
printf '%s\n' "$O" | grep -q '^20 90' && pass "...and follows it when resized (20 90)" || fail "after a resize: $(printf '%s\n' "$O" | grep -E '^[0-9]+ [0-9]+' | tr '\n' ' ')"
grep -q '^EXIT 5' "$T/out" && pass "bash's exit status is bash.exe's (5)" || fail "exit: $(tr -d '\r' < "$T/out" | tr '\n' ' ')"
# the console's escape sequences full-screen programs need: a scroll region,
# and repeat -- htop's meters and vi's screen were garbled without them
cp "$T/vtregion.exe" "$D/"
( cd "$D" && timeout -s KILL 60 "$WINE" 'C:\vtregion.exe' >/dev/null 2>&1 )
V=$(tr -d '\r' < "$D/vtregion-out.txt" 2>/dev/null)
[ "$(printf '%s\n' "$V" | sed -n 's/^ROW1 //p')" = line2 ] && [ "$(printf '%s\n' "$V" | sed -n 's/^ROW3 //p')" = "" ] \
    && [ "$(printf '%s\n' "$V" | sed -n 's/^ROW4 //p')" = line4 ] && [ "$(printf '%s\n' "$V" | sed -n 's/^ROW5 //p')" = line5 ] \
    && pass "a line feed at a scroll region's bottom scrolls the region only (CSI r)" || fail "scroll region: $(printf '%s' "$V" | tr '\n' '|')"
[ "$(printf '%s\n' "$V" | sed -n 's/^ROW0 //p')" = xxxxx ] && pass "CSI b repeats the last character (xxxxx)" || fail "repeat: $(printf '%s\n' "$V" | sed -n 's/^ROW0 //p')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
