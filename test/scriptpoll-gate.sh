#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A long-running script asks its host whether to go on (patches/sg/7831): JScript and VBScript call the
# site's IActiveScriptSiteInterruptPoll::QueryContinue about once a second, as Windows' engines do, and stop
# the script when it answers S_FALSE (a host's "stop running this script?"). test/scriptpoll-probe.c, 64- and
# 32-bit:
#   - a host that answers S_FALSE the third time: an endless loop is stopped after about 3 s, asked 3 times;
#   - a host that always answers S_OK: a 2-second loop runs to its end, the host asked;
#   - a host without the interface: the loop runs to its end.
#   WINE=/opt/wine-sg/bin/wine test/scriptpoll-gate.sh   (mutants SG_MUTANT_JS_NO_INTERRUPT_POLL, SG_MUTANT_VBS_NO_INTERRUPT_POLL)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for c in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$c" >/dev/null || { echo "SKIP: $c not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-scriptpoll.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/scriptpoll-probe.c" -lole32 -loleaut32 -luuid &&
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/scriptpoll-probe.c" -lole32 -loleaut32 -luuid ||
    { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for b in p64 p32; do
    timeout -s KILL 120 "$WINE" "$T/$b.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/$b.out"
    grep -q '^DONE' "$T/$b.out" || { fail "$b: the probe did not finish ($(tail -1 "$T/$b.out"))"; continue; }
    for e in js vbs; do
        l=$(sed -n "s/^${e}_stop=//p" "$T/$b.out"); set -- $l; ms=${3#ms=}
        [ "$1" = stopped ] && [ "$2" = asked=3 ] && [ "$ms" -ge 2500 ] && [ "$ms" -le 6000 ] &&
            pass "$b $e: an endless script is stopped when the host's QueryContinue says S_FALSE ($l)" ||
            fail "$b $e: the host's S_FALSE: '$l' (want stopped, asked=3, about 3000 ms)"
        l=$(sed -n "s/^${e}_go=//p" "$T/$b.out"); set -- $l
        [ "$1" = done ] && [ "${2#asked=}" -ge 1 ] && pass "$b $e: a host that says go on is asked, and the script runs to its end ($l)" ||
            fail "$b $e: a host that says go on: '$l'"
        l=$(sed -n "s/^${e}_none=//p" "$T/$b.out"); set -- $l
        [ "$1" = done ] && pass "$b $e: a host without the interface: the script runs to its end ($l)" || fail "$b $e: no interface: '$l'"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
