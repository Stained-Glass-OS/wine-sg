#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# error_category::default_error_condition and equivalent() (patches/sg/4507),
# 64- and 32-bit: test/msvcp-errcat-probe.c calls the virtuals of the
# categories msvcp100 exports, and of the ones in the system_error objects
# msvcp110..msvcp140 throw (_Throw_C_error, _XGetLastError; caught in a
# vectored handler), with a table of conditions and codes of the same and of
# another category, and
# checks that msvcp140's system category maps Windows errors with an errno
# equivalent to generic conditions (so ERROR_ACCESS_DENIED compares equal to
# errc::permission_denied). The three were FIXME stubs returning NULL/false,
# so every error_code == errc comparison was false.
#
#   WINE=/opt/wine-sg/bin/wine test/msvcp-errcat-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_ERRCAT_EQUIV_VALUE_ONLY (msvcp90/misc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msvcp-errcat.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/msvcp-errcat-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/msvcp-errcat-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for case in "exported msvcp100" "generic msvcp110" "generic msvcp120" "generic msvcp140" "generic msvcp_win" \
                "system msvcp140" "system msvcp_win"; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $case 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $case ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $case"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
