#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's crash is told in this system's words (patches/sg/0460).
#
# When a program crashed, the dialog said "a deficiency in Wine" and sent the
# user to WineHQ's application database and bug tracker, and the debugger
# retitled the program's console "Wine Debugger" (Steam's web helper, run as
# an administrator, showed that on the taskbar). The texts are ours now.
#
#   WINE=/opt/wine-sg/bin/wine test/crashbrand-gate.sh
#   WINEDBG=build/obj/programs/winedbg/x86_64-windows/winedbg.exe test/crashbrand-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
LIB="$(dirname "$WINE")/../lib/wine"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v strings >/dev/null || { echo "SKIP: no strings (binutils)"; exit 77; }
# a build tree (WINE=build/obj/wine, as the gates run in a dev tree) keeps
# winedbg.exe under programs/winedbg/<arch>-windows
[ -d "$LIB" ] || LIB="$(dirname "$WINE")/programs/winedbg"
set -- ${WINEDBG:-"$LIB/x86_64-windows/winedbg.exe" "$LIB/i386-windows/winedbg.exe"}
for exe; do
    [ -f "$exe" ] || { fail "no $exe"; continue; }
    name=$(basename "$(dirname "$exe")")/winedbg.exe
    wide=$(strings -el "$exe")
    if printf '%s\n' "$wide" | grep -qi "winehq\|deficiency in Wine\|WineDbg was"; then
        fail "$name: the crash dialog still names Wine: $(printf '%s\n' "$wide" | grep -i -m1 "winehq\|deficiency in Wine\|WineDbg was")"
    else
        pass "$name: the crash dialog does not send users to Wine's sites"
    fi
    printf '%s\n' "$wide" | grep -q "how Stained Glass OS" && pass "$name: it says what may be at fault in our words" || fail "$name: no Stained Glass OS text"
    strings -a "$exe" | grep -qx "Wine Debugger" && fail "$name: a console is still titled Wine Debugger" || pass "$name: no console is titled Wine Debugger"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
