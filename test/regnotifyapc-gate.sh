#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll/server registry watches with an APC routine (6082): NtNotifyChangeMultipleKeys/NtNotifyChangeKey ignored
# the APC (a FIXME) and refused a watch with no event, which is how GLib watches the registry (GTK programs: Geany,
# GIMP, Inkscape... GAppInfo, settings). Now the server ends the watch on the calling thread: ntdll fills the I/O
# status block (STATUS_SUCCESS on a change, STATUS_NOTIFY_CLEANUP when the handle is closed) and queues the APC
# routine as a user APC -- it runs in that thread's next alertable wait, not in a plain one. Event watches unchanged.
# The probe runs 64-bit and 32-bit (wow64).
#   WINE=/opt/wine-sg/bin/wine test/regnotifyapc-gate.sh   (mutants SG_MUTANT_REGAPC_NO_APC, SG_MUTANT_REGAPC_NO_CLEANUP)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-regnotifyapc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for arch in x86_64 i686; do
    if ! command -v $arch-w64-mingw32-gcc >/dev/null; then echo "SKIP  $arch: no compiler"; continue; fi
    TMPDIR=/var/tmp $arch-w64-mingw32-gcc -O1 -o "$T/probe-$arch.exe" "$HERE/regnotifyapc-probe.c" 2>"$T/cc.log" ||
        { cat "$T/cc.log"; fail "$arch probe did not build"; continue; }
    out=$(timeout -s KILL 120 "$WINE" "$T/probe-$arch.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    expect() {   # expect KEY VALUE WHAT
        if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$arch: $3"
        else fail "$arch: $3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
    }
    expect apc_register 0x103 "a watch with an APC routine and no event is pending (STATUS_PENDING)"
    expect apc_before_alertable 0 "a change does not run the routine in a plain wait"
    expect apc_calls 1 "the next alertable wait runs it once"
    expect apc_context 1 "with the caller's context"
    expect apc_iosb 1 "and I/O status block"
    expect apc_status 0 "which says STATUS_SUCCESS"
    expect apc_thread 1 "on the thread that set the watch"
    expect apc_once 1 "a fired watch stays over until set again"
    expect subtree_register 0x103 "NtNotifyChangeKey with an APC, a subtree watch"
    expect subtree_calls 1 "a new subkey ends it"
    expect subtree_iosb 1 "with its own status block"
    expect close_calls 1 "closing the key ends a pending watch"
    expect close_status 0x10b "as STATUS_NOTIFY_CLEANUP"
    expect both_event 1 "an event and an APC: the event is set"
    expect both_calls 1 "and the routine runs"
    expect event_signaled 1 "RegNotifyChangeKeyValue with an event works as before"
    expect event_calls 0 "and runs no routine"
    expect done 1 "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
