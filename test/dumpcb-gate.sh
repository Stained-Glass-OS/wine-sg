#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# MiniDumpWriteDump's callbacks the way Windows makes them (patches/sg/9551),
# 64- and 32-bit: test/dumpcb-probe.c. Only ThreadCallback and ModuleCallback
# were made: no IncludeThread/IncludeModule (leave a thread or module out),
# MemoryCallback/RemoveMemoryCallback (add, take out memory), CancelCallback,
# IoStartCallback (the caller takes the writes), and the rest Windows makes.
#
#   WINE=/opt/wine-sg/bin/wine test/dumpcb-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dbghelp/minidump.c): SG_MUTANT_NO_CANCEL, SG_MUTANT_NO_IO_CALLBACK,
# SG_MUTANT_NO_INCLUDE_THREAD, SG_MUTANT_NO_INCLUDE_MODULE,
# SG_MUTANT_NO_MEMORY_CALLBACK, SG_MUTANT_NO_REMOVE_MEMORY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dumpcb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -Wl,--build-id -o "$T/p64.exe" "$HERE/dumpcb-probe.c" -ldbghelp &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -Wl,--build-id -o "$T/p32.exe" "$HERE/dumpcb-probe.c" -ldbghelp || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
