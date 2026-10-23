#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mscoree: Wine Mono's soft debugger agent (6074, NET-DBG phase 0), strictly opt-in. With WINE_MONO_DEBUGGER_AGENT
# set by the user (transport=dt_socket,address=127.0.0.1:PORT,server=y,suspend=y) a managed program waits for a
# debugger that speaks Mono's soft debugger protocol (dnSpy's Mono debugger, VS Code mono-debug); the gate's
# Python client does the handshake, VERSION, ALL_THREADS and RESUME, and the program then runs. An address that is
# not loopback is refused: the program runs at once with no agent; unset, nothing listens.
#
# DEVELOPER-ONLY SWITCH: a loopback TCP port can be reached by every local user -- on a multi-user machine alice
# can connect to bob's agent and run code in his process. Nothing in Stained Glass OS sets the variable.
#
# Needs Wine Mono (its mcs.exe builds the program) and python3. SKIP without.
#   WINE=/opt/wine-sg/bin/wine test/monoagent-gate.sh   (mutant SG_MUTANT_AGENT_ANY_ADDRESS)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v python3 >/dev/null || { echo "SKIP: python3 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
MCS=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
    [ -n "$f" ] && MCS=$f
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-monoagent.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="winemenubuilder.exe=d" DISPLAY=
unset WINE_MONO_DEBUGGER_AGENT
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/monoagent-app.cs" "$T/app.cs"
(cd "$T" && timeout -s KILL 300 "$WINE" "$MCS" -debug -out:App.exe app.cs >/dev/null 2>&1 </dev/null)
[ -f "$T/App.exe" ] || { fail "mcs did not build the program"; exit 1; }
cp "$T/App.exe" "$C/App.exe"
PORT=$((20000 + $$ % 20000))
run_app() {   # run_app AGENT-OPTIONS: the program's exit code in $T/rc
    rm -f "$C/app-ran.txt"
    (cd "$C" && WINE_MONO_DEBUGGER_AGENT="$1" timeout -s KILL 120 "$WINE" 'C:\App.exe' >/dev/null 2>&1 </dev/null; echo $? > "$T/rc") &
    APP=$!
}

# with the agent: the program waits for the debugger
run_app "transport=dt_socket,address=127.0.0.1:$PORT,server=y,suspend=y"
sleep 3
[ -e "$C/app-ran.txt" ] && fail "the program ran before a debugger resumed it" || pass "the program waits for a debugger"
out=$(timeout 60 python3 "$HERE/monoagent-client.py" "$PORT")
wait "$APP"
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Connect 1 "the agent listens on the loopback port"
expect Handshake 1 "the soft debugger handshake"
expect Version "0 1 1" "VM VERSION: a Mono VM"
expect Threads "0 1" "VM ALL_THREADS"
expect Resume 0 "VM RESUME"
[ -e "$C/app-ran.txt" ] && [ "$(cat "$T/rc")" = 5 ] && pass "the program then runs to its exit code" || fail "the program did not run (exit $(cat "$T/rc"))"

# not loopback: refused, the program runs with no agent
PORT2=$((PORT + 1))
run_app "transport=dt_socket,address=192.0.2.1:$PORT2,server=y,suspend=y"   # TEST-NET-1: never exposed, even by a broken check
wait "$APP"
[ -e "$C/app-ran.txt" ] && [ "$(cat "$T/rc")" = 5 ] && pass "an address that is not loopback is refused: no agent, the program runs" \
    || fail "with 192.0.2.1 the program did not run at once (exit $(cat "$T/rc"))"
if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(1); sys.exit(0 if s.connect_ex(('127.0.0.1', $PORT2)) else 1)"; then
    pass "nothing listens"; else fail "something listens on $PORT2"; fi

# unset: no agent
run_app ""
wait "$APP"
[ "$(cat "$T/rc")" = 5 ] && pass "without the variable the program just runs" || fail "without the variable: exit $(cat "$T/rc")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
