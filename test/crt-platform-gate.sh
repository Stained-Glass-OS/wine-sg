#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# __GetPlatformExceptionInfo and Concurrency::details::_ReportUnobservedException
# (patches/sg/4501), 64- and 32-bit. test/crt-platform-probe.c makes C++
# exception records current through ucrtbase's __current_exception and checks
# a table: bad_alloc (or a type deriving from it) sets *is_bad_alloc, other
# types, non-C++ exceptions and WinRT throw infos do not, no information is
# returned (ucrtbase, vcruntime140 forwarder; msvcr120 export without a current
# exception). _ReportUnobservedException (msvcr110, msvcr120, msvcp140,
# msvcp_win forwarder) must terminate the program (abort, exit code 3). All were stubs
# (Edge, Audacity).
#
#   WINE=/opt/wine-sg/bin/wine test/crt-platform-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_PLATFORM_NO_BADALLOC (msvcrt/except.c),
# SG_MUTANT_UNOBSERVED_RETURNS (msvcrt/concurrency.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-crt-platform.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/crt-platform-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/crt-platform-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in ucrtbase vcruntime140 msvcr120; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" info $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then
            echo "ok   $p info $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p info $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
    for dll in msvcr110 msvcr120 msvcp140 msvcp_win; do
        (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" unobserved $dll >"$T/out.txt" 2>/dev/null </dev/null)
        status=$?
        # terminate() ends in abort(): exit code 3 (a stub or a crash gives another)
        if grep -q 'RETURNED\|FAIL' "$T/out.txt" || [ "$status" != 3 ]; then
            echo "FAIL $p unobserved $dll: exit code $status"; RC=1
        else
            echo "ok   $p unobserved $dll terminated (exit code 3)"
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
