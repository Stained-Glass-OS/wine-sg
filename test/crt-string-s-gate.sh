#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CRT string functions (patches/sg/4511), 64- and 32-bit: _strset_s,
# __wcsncnt, _strerror_s, _umask_s, _lsearch_s, _mbsninc_l, _mbsstr_l,
# _mbstrnlen, _mbsset_s, _mbsnset_s, _mbsnbset_s, _mbsncat_s (msvcr90,
# msvcr120, ucrtbase; with an invalid parameter handler that returns), and
# _mbsncoll/_mbsnicoll (msvcrt too): test/crt-string-s-probe.c, including
# code page 932 double-byte cases. All were spec stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/crt-string-s-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_LSEARCH_NO_APPEND (msvcrt/misc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-crt-strs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/crt-string-s-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/crt-string-s-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in msvcrt msvcr90 msvcr120 ucrtbase; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
