#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Minidump streams crash reporters ask for (patches/sg/9550), 64- and 32-bit:
# test/dumpstreams-probe.c. MiniDumpWithThreadInfo, MiniDumpWithFullMemoryInfo
# and MiniDumpWithUnloadedModules wrote nothing: no ThreadInfoListStream
# (thread times, start addresses), no MemoryInfoListStream (the address
# space's regions) and no UnloadedModuleListStream (what the process had
# unloaded -- a crash in an unloaded DLL is unreadable without it). Checked
# for the probe itself and for a child process, parsed by hand.
#
#   WINE=/opt/wine-sg/bin/wine test/dumpstreams-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dbghelp/minidump.c): SG_MUTANT_NO_THREAD_INFO,
# SG_MUTANT_NO_MEMORY_INFO, SG_MUTANT_NO_UNLOADED_STREAM,
# SG_MUTANT_NO_UNLOADED_MODULES (the stream, empty).
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
T=$(mktemp -d /var/tmp/sg-dumpstreams.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -Wl,--build-id -o "$T/p64.exe" "$HERE/dumpstreams-probe.c" -ldbghelp &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -Wl,--build-id -o "$T/p32.exe" "$HERE/dumpstreams-probe.c" -ldbghelp || { echo "FAIL  probe did not build"; exit 1; }
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
