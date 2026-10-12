#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Small msvcrt-family stubs (patches/sg/4505), 64- and 32-bit:
# _get_FMA3_enable/_set_FMA3_enable (64-bit ucrtbase, msvcr120) report that no
# FMA3 implementations are in use; __clean_type_info_names_internal (msvcr80,
# msvcr90) has no list to free; __security_error_handler without a handler
# (msvcr70, msvcr71) reports the buffer overrun on stderr and ends the program
# with exit code 3 (the message was a FIXME).
#
#   WINE=/opt/wine-sg/bin/wine test/crt-misc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_SECERR_SILENT (msvcrt/except.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-crt-misc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/crt-misc-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/crt-misc-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for mode in fma typeinfo; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $mode 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then echo "ok   $p $mode"; else echo "FAIL $p $mode"; printf '%s\n' "$out" | grep -v '^ok'; RC=1; fi
    done
    for dll in msvcr70 msvcr71; do
        for code in 1 0; do
            (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" secerr $dll $code >"$T/out.txt" 2>"$T/err.txt" </dev/null)
            status=$?
            if [ "$code" = 1 ]; then msg="Buffer overrun detected!"; else msg="Unknown security failure detected!"; fi
            if [ "$status" = 3 ] && grep -q "$msg" "$T/err.txt" && ! grep -q RETURNED "$T/out.txt"; then
                echo "ok   $p $dll security error $code: \"$msg\", exit code 3"
            else
                echo "FAIL $p $dll security error $code: exit code $status, stderr: $(tr -d '\r' < "$T/err.txt" | grep -v ':fixme:\|:err:' | head -2)"; RC=1
            fi
        done
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
