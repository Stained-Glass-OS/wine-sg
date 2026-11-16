#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mscoree + Wine Mono: the soft debugger agent reachable by the user alone (NET-DBG phase 1, wine-sg 7791,
# David's choice (c)). With WINE_MONO_DEBUGGER_AGENT=transport=dt_unix,server=y,suspend=y a managed program waits
# for a debugger on a Unix socket of the user's own ($XDG_RUNTIME_DIR/wine-mono-debug/PID.sock, opened in the
# program's process by sgdbgsock.dll):
#   - the directory is 0700 and the socket 0600, both the user's;
#   - a second user (the harness's sgconf) cannot connect;
#   - even with the path opened to them (chmod by the owner), their connection is closed unanswered: the agent
#     takes the same user only (SO_PEERCRED);
#   - the owner's debugger (the gate's Python SDB client) does the handshake, VM VERSION, ALL_THREADS, RESUME,
#     the program runs to its exit code and the socket is gone;
#   - there is no TCP transport at all: transport=dt_socket on 127.0.0.1 is refused, nothing listens, the
#     program runs at once; unset, nothing either.
#
# Needs Wine Mono with the dt_unix transport (SG_MONO_RUNTIME=DIR names one: a wine-mono-* directory, used through
# HKCU\Software\Wine\Mono RuntimePath), python3, and sudo to the second user (SG_OTHER, default sgconf). SKIP without.
#   WINE=/opt/wine-sg/bin/wine test/monoagent-gate.sh   (MONOAGENT_MCSFLAGS=-platform:x86: a 32-bit program)
#   mutants: SG_MUTANT_DBGSOCK_ANY_PEER, SG_MUTANT_DBGSOCK_OPEN_PERMS (sgdbgsock unixlib.c), SG_MUTANT_AGENT_TCP (mscoree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
SG_OTHER=${SG_OTHER:-sgconf}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v python3 >/dev/null || { echo "SKIP: python3 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
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
T=$(mktemp -d /var/tmp/sg-monoagent.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="winemenubuilder.exe=d" DISPLAY=
unset WINE_MONO_DEBUGGER_AGENT
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RTDOS="Z:$(printf '%s' "$RUNTIME" | tr '/' '\\')"
"$WINE" reg add 'HKCU\Software\Wine\Mono' /v RuntimePath /d "$RTDOS" /f >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/monoagent-app.cs" "$T/app.cs"
(cd "$T" && timeout -s KILL 300 "$WINE" "$MCS" -debug ${MONOAGENT_MCSFLAGS:-} -out:App.exe app.cs >/dev/null 2>&1 </dev/null)
[ -f "$T/App.exe" ] || { fail "mcs did not build the program"; exit 1; }
cp "$T/App.exe" "$C/App.exe"
run_app() {   # run_app AGENT-OPTIONS: the program's exit code in $T/rc
    rm -f "$C/app-ran.txt"
    (cd "$C" && WINE_MONO_DEBUGGER_AGENT="$1" timeout -s KILL 120 "$WINE" 'C:\App.exe' >/dev/null 2>&1 </dev/null; echo $? > "$T/rc") &
    APP=$!
}
DIR="$XDG_RUNTIME_DIR/wine-mono-debug"

# the agent on the user's socket: the program waits for the debugger
run_app "transport=dt_unix,server=y,suspend=y"
SOCK=""
for i in $(seq 150); do SOCK=$(ls "$DIR"/*.sock 2>/dev/null | head -1); [ -n "$SOCK" ] && break; sleep 0.2; done
[ -n "$SOCK" ] || { fail "no socket in $DIR"; wait "$APP"; exit 1; }
[ -e "$C/app-ran.txt" ] && fail "the program ran before a debugger resumed it" || pass "the program waits for a debugger"
dmode=$(stat -c '%a %U' "$DIR"); smode=$(stat -c '%a %U %F' "$SOCK")
if [ "$dmode" = "700 $(id -un)" ] && [ "$smode" = "600 $(id -un) socket" ]; then
    pass "the socket is the user's alone: directory $dmode, socket $smode"
else fail "permissions: directory '$dmode', socket '$smode' (want 700 and 600, the user's)"; fi

# another user: cannot reach it
other=$(sudo -n -u "$SG_OTHER" timeout 20 python3 "$HERE/monoagent-client.py" "$SOCK" probe 2>&1)
if printf '%s\n' "$other" | grep -qx 'Connect=0'; then pass "$SG_OTHER cannot connect ($(printf '%s' "$other" | tr '\n' ' '))"
else fail "$SG_OTHER reached the socket: $(printf '%s' "$other" | tr '\n' ' ')"; fi

# the path opened to them by its owner: their connection is closed unanswered (SO_PEERCRED)
for d in "$SG_GATE_HOME" "$XDG_RUNTIME_DIR" "$DIR"; do chmod 711 "$d"; done
chmod 666 "$SOCK"
other=$(sudo -n -u "$SG_OTHER" timeout 20 python3 "$HERE/monoagent-client.py" "$SOCK" probe 2>&1)
chmod 700 "$SG_GATE_HOME" "$XDG_RUNTIME_DIR" "$DIR"; chmod 600 "$SOCK"
if printf '%s\n' "$other" | grep -qx 'Handshake=1'; then
    fail "with the path open, $SG_OTHER got the agent's handshake: $(printf '%s' "$other" | tr '\n' ' ')"
else pass "with the path open, $SG_OTHER is turned away by the agent itself ($(printf '%s' "$other" | tr '\n' ' '))"; fi

# the owner's debugger
out=$(timeout 60 python3 "$HERE/monoagent-client.py" "$SOCK")
wait "$APP"
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Connect 1 "the owner connects"
expect Handshake 1 "the soft debugger handshake"
expect Version "0 1 1" "VM VERSION: a Mono VM"
expect Threads "0 1" "VM ALL_THREADS"
expect Resume 0 "VM RESUME"
[ -e "$C/app-ran.txt" ] && [ "$(cat "$T/rc")" = 5 ] && pass "the program then runs to its exit code" || fail "the program did not run (exit $(cat "$T/rc"))"
[ -e "$SOCK" ] && fail "the socket is still there after the program" || pass "the socket is gone with the program's connection"

# no TCP, not even loopback
PORT=$((20000 + $$ % 20000))
run_app "transport=dt_socket,address=127.0.0.1:$PORT,server=y,suspend=y"
wait "$APP"
[ -e "$C/app-ran.txt" ] && [ "$(cat "$T/rc")" = 5 ] && pass "transport=dt_socket is refused: no agent, the program runs" \
    || fail "with dt_socket the program did not run at once (exit $(cat "$T/rc"))"
if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(1); sys.exit(0 if s.connect_ex(('127.0.0.1', $PORT)) else 1)"; then
    pass "nothing listens on TCP"; else fail "something listens on 127.0.0.1:$PORT"; fi

# unset: no agent
run_app ""
wait "$APP"
[ "$(cat "$T/rc")" = 5 ] && pass "without the variable the program just runs" || fail "without the variable: exit $(cat "$T/rc")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
