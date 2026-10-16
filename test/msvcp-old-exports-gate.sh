#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Exports of the old C++ runtimes wired to existing code (patches/sg/4508),
# 64- and 32-bit: msvcm80/msvcm90 entries forward to msvcp80/msvcp90 (and the
# C functions of their mangled managed copies), msvcp70 complex operators,
# _LCosh/_LSinh of msvcp60..90, msvcp110/msvcp120 _Throw_Cpp_error. Smoke gate
# (fast mode): test/msvcp-old-exports-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/msvcp-old-exports-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msvcp-old.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/msvcp-old-exports-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/msvcp-old-exports-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for case in forwards complex "cpperror msvcp110" "cpperror msvcp120"; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $case 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then echo "ok   $p $case"
        else echo "FAIL $p $case"; printf '%s\n' "$out" | grep -v '^ok'; RC=1; fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
