#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ICorDebug over Wine Mono's soft debugger agent (wine-sg 7792/7796, NET-DBG phase 2: processes, threads,
# stacks and values).
# A debugger (cordbgsdb-probe.exe, raw ICorDebug) and a managed debuggee (cordbgsdb-app.cs: a background
# worker and the main thread counting into C:\counter-PID.txt):
#   launch    ICorDebug::CreateProcess: the CreateProcess callback; Continue starts it; Stop stops its
#             managed threads (the count holds); EnumerateThreads gives them (at least two, each a thread
#             of the debuggee, the worker background); GetThread finds one by id; a thread's chain and IL frames
#             (7796): the frame of App.Inspect, its token, its locals (long, double, bool, string) and arguments
#             (this, int, string) as they are, out-of-range indexes refused; Continue: running, the
#             count goes on; EnumerateThreads while running: CORDBG_E_PROCESS_NOT_SYNCHRONIZED
#   attach    a program running its agent (dt_unix, server, not suspended): DebugActiveProcess stops it,
#             its threads are enumerated; Detach lets it run on, alive
#   no agent  DebugActiveProcess on a program without one: CORDBG_E_DEBUGGING_NOT_POSSIBLE
# The agent is reached on the user's own socket (sgdbgsock.dll, 7791).
#
# Needs Wine Mono with the dt_unix transport (SG_MONO_RUNTIME=DIR names one) and mingw. SKIP without.
#   WINE=/opt/wine-sg/bin/wine test/cordbgsdb-gate.sh
#   mutants: SG_MUTANT_CDB_STOP_NOOP, SG_MUTANT_CDB_THREAD_ID, SG_MUTANT_CDB_NO_AGENT, SG_MUTANT_CDB_ATTACH_RUNNING,
#            SG_MUTANT_CDB_VALUE_ZERO, SG_MUTANT_CDB_ARG_SHIFT (mscoree cordebug.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
MCS=""; RUNTIME=""
for d in "${SG_MONO_RUNTIME:-}" "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    for m in "$d" "$d"/wine-mono-*; do
        [ -f "$m/lib/mono/4.5/mcs.exe" ] || continue
        [ -n "$MCS" ] || MCS="$m/lib/mono/4.5/mcs.exe"
        [ -z "$RUNTIME" ] && grep -q mono_sg_debugger_unix_transport "$m/bin/libmono-2.0-x86_64.dll" 2>/dev/null && RUNTIME=$m
    done
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
[ -n "$RUNTIME" ] || { echo "SKIP: no Wine Mono with the dt_unix transport (SG_MONO_RUNTIME)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cordbgsdb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="winemenubuilder.exe=d" DISPLAY=
unset WINE_MONO_DEBUGGER_AGENT
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
"$MINGW" -O1 -o "$T/probe.exe" "$HERE/cordbgsdb-probe.c" -lole32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Mono' /v RuntimePath /d "Z:$(printf '%s' "$RUNTIME" | tr '/' '\\')" /f >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/cordbgsdb-app.cs" "$T/app.cs"
(cd "$T" && timeout -s KILL 300 "$WINE" "$MCS" -debug -out:App.exe app.cs >/dev/null 2>&1 </dev/null)
[ -f "$T/App.exe" ] || { fail "mcs did not build the program"; exit 1; }
cp "$T/App.exe" "$T/probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 300 "$WINE" probe.exe 'C:\App.exe' 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Launch "0 1" "CreateProcess, and its callback"
expect Launch.Continue 0 "Continue starts it"
expect Launch.Stop 0 "Stop"
expect Launch.Holds 1 "stopped, its managed threads do not run"
expect Launch.EnumerateThreads 0 "EnumerateThreads"
expect Launch.Threads "1 1 1" "its threads: two or more, each the debuggee's, the worker background"
expect Launch.GetThread "0 1" "GetThread by id"
expect Inspect 1 "a thread's chain and frames: the frame whose argument 1 is 12345 (App.Inspect)"
expect Inspect.Token 1 "its function's token is a MethodDef"
expect Value.big "0xa 1234567890123" "local 0: long 1234567890123"
expect Value.ratio "0xd 2.50" "local 1: double 2.5"
expect Value.flag "0x2 1" "local 2: bool true"
expect Value.text "0xe Grüße ✓" "local 3: a string, dereferenced: Grüße ✓"
expect Value.label "0xe sg-label" "argument 2: a string"
expect Value.this "0x12 object" "argument 0: this, an object reference"
expect Inspect.BadLocal 1 "a local that is not there: E_INVALIDARG (and the debuggee lives)"
expect Inspect.BadArg 1 "an argument that is not there: E_INVALIDARG"
expect Launch.Continue2 0 "Continue"
expect Launch.IsRunning 1 "running again"
expect Launch.Runs 1 "and its threads run"
expect Launch.EnumWhileRunning 1 "EnumerateThreads while running: not synchronized"
expect Before.Runs 1 "a program with its agent runs"
expect Attach "0 2" "DebugActiveProcess, and the CreateProcess callback"
expect Attach.Holds 1 "attached, it is stopped"
expect Attach.Threads "1 1 1" "its threads"
expect Attach.Detach 0 "Detach"
expect Detached.Runs 1 "detached, it runs on"
expect Detached.Alive 1 "and lives"
expect NoAgent 1 "no agent: debugging not possible"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
