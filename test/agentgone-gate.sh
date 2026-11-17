#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A thread the server cannot reach is not waited for forever (wine-sg 7843).
# In a shared prefix the machine's server runs as SYSTEM, so it stops a user's thread (SuspendThread +
# GetThreadContext, which every .NET garbage collection does) through that user's sg-procagent (0021).
# With the agent gone the signal never went out and GetThreadContext waited for good: MSBuild and every
# other .NET program hung at its next collection. Now the request fails (ERROR_ACCESS_DENIED) at once and
# the next one tries the agent again. Nothing else reaches the thread: no signal from another user.
# With the standard user $SG_USER1 and SYSTEM ($SG_SYSTEM, the prefix owner):
#   agent running: GetThreadContext of a suspended busy thread succeeds; a .NET program collects garbage
#   agent killed while a .NET program collects garbage: the program still finishes
#   no agent: GetThreadContext fails with 5 in well under the agent's 10 s, the process goes on
#   agent back: GetThreadContext succeeds again
#   WINE=... test/agentgone-gate.sh   (SG_MONO_RUNTIME=DIR: a wine-mono-* directory; SG_PROCAGENT=path)
# Mutant: server SG_MUTANT_UNREACHABLE_SUSPEND_PENDS (thread.c).
# Needs passwordless sudo, the accounts of usersessions-gate.sh, mingw, Wine Mono and sg-procagent; exit 77 otherwise.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_GROUP=${SG_GROUP:-sgwine}
AGENT=${SG_PROCAGENT:-/usr/libexec/stained-glass/sg-procagent}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: no mingw"; exit 77; }
[ -x "$AGENT" ] || { echo "SKIP: no sg-procagent at $AGENT"; exit 77; }
MCS=""
for d in "${SG_MONO_RUNTIME:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    for m in "$d" "$d"/wine-mono-*; do
        [ -f "$m/lib/mono/4.5/mcs.exe" ] && { MCS=$m/lib/mono/4.5/mcs.exe; RUNTIME=$m; break 2; }
    done
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
W=$(mktemp -d /var/tmp/sg-agentgone.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" XDG_RUNTIME_DIR="$W/run-$u" \
        WAYLAND_DISPLAY=sg-no-wayland DISPLAY= SG_WINE_REDIRECT=0 "$@"
}
agent_start() {
    sudo -n -u "$SG_USER1" rm -f "$W/u1/agent.pid"
    as "$SG_USER1" sh -c 'echo $$ > "$1"; exec "$2" "$3"' _ "$W/u1/agent.pid" "$AGENT" "$PFX" >/dev/null 2>&1 </dev/null &
    for _ in $(seq 50); do [ -S "$PFX/.sg-procagent.$(id -u "$SG_USER1")" ] && [ -s "$W/u1/agent.pid" ] && return 0; sleep 0.2; done
    return 1
}
agent_kill() {   # by its PID only
    local p; p=$(cat "$W/u1/agent.pid" 2>/dev/null) || return 0
    [ -n "$p" ] && sudo -n -u "$SG_USER1" kill "$p" 2>/dev/null
    for _ in $(seq 25); do sudo -n -u "$SG_USER1" kill -0 "$p" 2>/dev/null || break; sleep 0.2; done
    sudo -n -u "$SG_USER1" rm -f "$W/u1/agent.pid"
}
cleanup() {
    set +e
    agent_kill
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

x86_64-w64-mingw32-gcc -O1 -o "$W/probe.exe" "$HERE/agentgone-probe.c" || { echo "FAIL build"; exit 1; }
cp "$HERE/agentgone-app.cs" "$W/app.cs"
for u in "$SG_SYSTEM" "$SG_USER1"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
sudo -n install -d -o "$SG_USER1" -m 755 "$W/u1"
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
RTDOS="Z:$(printf '%s' "$RUNTIME" | tr '/' '\\')"
as "$SG_USER1" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Mono' /v RuntimePath /d "$RTDOS" /f >/dev/null 2>&1
as "$SG_USER1" sh -c "cd '$W/u1' && cp '$W/probe.exe' '$W/app.cs' . && timeout -s KILL 300 '$WINE' '$MCS' -out:App.exe app.cs" >/dev/null 2>&1 </dev/null
[ -f "$W/u1/App.exe" ] || { fail "mcs did not build the .NET program"; exit 1; }

probe() {   # probe NAME: run the native probe as the user, output in $W/NAME.out
    as "$SG_USER1" sh -c "cd '$W/u1' && timeout -s KILL 60 '$WINE' probe.exe" > "$W/$1.out" 2>/dev/null </dev/null
    tr -d '\r' < "$W/$1.out" > "$W/$1.txt"
}
val() { sed -n "s/^$2=//p" "$W/$1.txt" | head -1; }
app() {     # app NAME SECONDS: the .NET program, bounded
    as "$SG_USER1" sh -c "cd '$W/u1' && timeout -s KILL $(($2 + 90)) '$WINE' App.exe $2" > "$W/$1.out" 2>/dev/null </dev/null
    echo $? > "$W/$1.rc"
    tr -d '\r' < "$W/$1.out" > "$W/$1.txt"
}

# 1. the agent running: the thread is reached
agent_start || { fail "sg-procagent did not start"; exit 1; }
probe withagent
[ "$(val withagent ctx)" = OK ] && pass "with the agent, GetThreadContext of the suspended thread succeeds" \
    || fail "with the agent: ctx='$(val withagent ctx)' (the harness cannot prove anything)"
app gcagent 8
[ -n "$(val gcagent gc)" ] && pass "with the agent, the .NET program collects garbage ($(val gcagent gc) collections)" \
    || fail "with the agent the .NET program did not finish (exit $(cat "$W/gcagent.rc"))"

# 2. the agent killed while a .NET program collects garbage
app gckill 25 &
APP=$!
sleep 8
agent_kill
wait "$APP"
if [ -n "$(val gckill gc)" ]; then pass "the agent killed mid-run: the .NET program still finishes ($(val gckill gc) collections)"
else fail "the agent killed mid-run: the .NET program hung (exit $(cat "$W/gckill.rc"), killed by the gate's bound)"; fi

# 3. no agent: a prompt failure, not a wait
probe noagent
ms=$(val noagent ms)
if [ "$(val noagent ctx)" = "FAIL 5" ] && [ -n "$ms" ] && [ "$ms" -lt 5000 ]; then
    pass "without the agent, GetThreadContext fails with 5 after ${ms} ms, and the process goes on"
else fail "without the agent: ctx='$(val noagent ctx)' ms='$ms' (wanted FAIL 5, < 5000 ms; nothing: it hung)"; fi

# 4. the agent back: the next suspend reaches the thread again
agent_start || fail "sg-procagent did not restart"
probe again
[ "$(val again ctx)" = OK ] && pass "the agent back: GetThreadContext succeeds again" \
    || fail "the agent back: ctx='$(val again ctx)'"

[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
