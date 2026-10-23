#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dbgeng dump targets (6073, DBG-DUMP): IDebugClient::OpenDumpFile(Wide) gave E_NOTIMPL. A child crashes and its
# exception handler writes a minidump with dbghelp (MiniDumpWithDataSegs); dbgeng opens it:
#  - a missing file and a file that is no dump fail; WaitForEvent reports the dump's exception once (code, address,
#    parameters), then E_UNEXPECTED; the debuggee is a small user dump;
#  - the threads (the current one is the crashing one), the modules (the probe at its base), the data the child wrote
#    (data segments) and no memory the dump lacks; the crashing thread's registers; SetThreadContext refused;
#  - a stack walk from the dump's memory (unwind data the dump left out comes from the module image file, as a
#    debugger reads it), its first frame at the exception, named crash_here by the symbols;
#  - GetNumberDumpFiles/GetDumpFile name the file; EndSession lets it go.
#   WINE=/opt/wine-sg/bin/wine test/dumpread-gate.sh   (mutants SG_MUTANT_DUMP_NO_MEMORY, SG_MUTANT_DUMP_NO_IMAGES)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dumpread.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O1 -g -o "$T/dumpread-probe.exe" "$HERE/dumpread-probe.c" -ldbgeng -ldbghelp || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/dumpread-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 180 "$WINE" 'C:\dumpread-probe.exe' 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Child 1 "the child crashed and wrote its minidump"
expect OpenMissing 0x80070002 "OpenDumpFile of a missing file fails"
expect OpenNotDump 0x8007000b "and of a file that is no minidump"
expect Open 0 "OpenDumpFile opens the dump"
expect Wait 0 "WaitForEvent reports its event"
expect Event "0x1 2 0xc0000005 1 1" "the access violation (DEBUG_EVENT_EXCEPTION): code, address, the address it wrote; S_FALSE for the absent description buffer"
expect Type "2 1024" "a small user-mode dump (DEBUG_CLASS_USER_WINDOWS, DEBUG_USER_WINDOWS_SMALL_DUMP)"
expect Threads "1 1" "its threads, the crashing one current"
expect Modules "1 1" "its modules, the probe at its base"
expect Memory "0 0x5ae1dead" "the data the child wrote, from the dump's data segments"
expect MemoryNone 1 "memory the dump lacks cannot be read"
expect Context "0 1" "the crashing thread's registers"
expect SetContext 0x8000ffff "a dump's registers do not change"
expect Stack "0 1 1 1" "a stack walk through the dump's memory, at crash_here"
expect DumpFile "1 0 1 0xffffffff" "GetNumberDumpFiles and GetDumpFile"
expect WaitAgain 0x8000ffff "nothing more happens in a dump"
expect End 0 "EndSession"
expect AfterEnd 0 "and the dump is let go"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
