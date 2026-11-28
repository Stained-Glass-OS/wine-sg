#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A global hook removed while it is being called is removed once (patches/sg/9655).
#
# A hook removed while its chain is in use stays in the chain, marked dead,
# until the chain is released; its threads' counts were already taken down.
# Wine then took them down again: a second UnhookWindowsHookEx of the same
# handle succeeded (Windows: FALSE, ERROR_INVALID_HOOK_HANDLE), the owner
# thread's end removed the dead hook once more, and a thread leaving the
# desktop subtracted it too (while a thread joining counted it). A count below
# zero aborted the machine's wineserver -- every Windows program of every user
# and every service -- when TightVNC's 32-bit hookldr.exe ended (9628 clamps
# it to zero and says so on stderr; this gate reads that line).
# test/hookcount-probe.c (scenarios A, B, C, D) + test/hookcount-dll.c.
#   WINE=/opt/wine-sg/bin/wine test/hookcount-gate.sh
# Mutant: SG_MUTANT_HOOK_REMOVE_TWICE (server/hook.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-hookcount.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -shared -o "$T/hookcount-dll.dll" "$HERE/hookcount-dll.c" -luser32 || { fail "dll did not build"; exit 1; }
"$MINGW" -O2 -o "$T/hookcount-probe.exe" "$HERE/hookcount-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# run SCENARIO: the probe under a server of its own whose stderr we keep
run() {
    "$WINESERVER" -f -p5 2>"$T/server-$1.err" &
    _srv=$!
    python3 -c "import time; time.sleep(1)"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/hookcount-probe.exe" "$1" 2>/dev/null | tr -d '\r')
    "$WINESERVER" -k 2>/dev/null
    timeout 30 sh -c "while kill -0 $_srv 2>/dev/null; do python3 -c 'import time; time.sleep(0.2)'; done"
    printf '%s\n' "$out" | sed "s/^/  $1: /"
}
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
balanced() {
    if grep -q 'below zero\|Assertion' "$T/server-$1.err"; then
        fail "$1: $2 -- the server's hook count went below zero: $(grep -m1 'below zero\|Assertion' "$T/server-$1.err")"
    else
        pass "$1: $2 -- every thread's hook count stayed balanced"
    fi
    [ "$(v D_CALLED)" = 1 ] && pass "$1: a new global hook is called afterwards" \
        || fail "$1: a new global hook was not called afterwards (D_CALLED '$(v D_CALLED)')"
}

run A
[ "$(v HOOK)" = 1 ] && [ "$(v A_FIRST)" = 1 ] || fail "A: the hook was not set or removed (HOOK '$(v HOOK)' A_FIRST '$(v A_FIRST)')"
[ "$(v A_SECOND)" = "0 1404" ] && pass "A: a second UnhookWindowsHookEx of a hook being called fails, ERROR_INVALID_HOOK_HANDLE" \
    || fail "A: a second UnhookWindowsHookEx gave '$(v A_SECOND)' (Windows: 0 1404)"
balanced A "UnhookWindowsHookEx twice inside the hook"

run B
[ "$(v HOOK)" = 1 ] && [ "$(v B_UNHOOK)" = 1 ] && [ "$(v B_ENDED)" = 1 ] \
    || fail "B: setup ('$(v HOOK)' '$(v B_UNHOOK)' '$(v B_ENDED)')"
balanced B "the owner thread unhooked and ended while the hook was being called"

run C
[ "$(v HOOK)" = 1 ] && [ "$(v C_UNHOOK)" = 1 ] && [ "$(v C_MOVED)" = 1 ] && [ "$(v C_ENDED)" = 1 ] \
    || fail "C: setup ('$(v HOOK)' '$(v C_UNHOOK)' '$(v C_MOVED)' '$(v C_ENDED)')"
balanced C "a thread moved to another desktop while the removed hook was still in the chain"

exit $RC
