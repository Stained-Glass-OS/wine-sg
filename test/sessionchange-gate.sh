#!/bin/sh
# Session events for services (patches/sg/9634, i1011). A service that accepts
# SERVICE_CONTROL_SESSIONCHANGE is told when a person signs in to a session
# (WTS_SESSION_LOGON, after WTS_CONSOLE_CONNECT at the console) and when the
# session ends (WTS_SESSION_LOGOFF), with a WTSSESSION_NOTIFICATION naming it,
# as Windows' session manager has the SCM tell it. Nobody told services:
# WireGuard's manager service, started at boot, never started its tray window
# for the person who then signed in. winlogon.exe, which sg-lockd starts as
# SYSTEM in each session, tells the SCM (I_ScSendTSMessage). Proven here:
#   - a session's winlogon.exe (started as sg-lockd does) makes the test
#     service get CONSOLE_CONNECT and LOGON for the person's session, and
#     LOGOFF when its standard input ends;
#   - the sign-in screen's session (its account is a system one) is no logon;
#   - a person's own program cannot send session events (only SYSTEM).
# Runs INSIDE a QA VM, as root, with a person signed in at the console:
#   SG_SC_USER=alice sh test/sessionchange-gate.sh
# SG_SC_PROBE: sessionchange-probe.exe built from test/sessionchange-probe.c
# (default: built here with mingw, else beside this script). 77: not in a VM
# with a session. Mutants: SG_MUTANT_NO_SESSION_EVENTS, SG_MUTANT_SESSION_EVENT_ANY
# (programs/services/rpc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
U=${SG_SC_USER:?SG_SC_USER}
SYS=${SG_SYSTEM_USER:-sgsystem}
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
nap() { python3 -c "import time; time.sleep($1)"; }
[ "$(id -u)" -eq 0 ] || { echo "SKIP: root only"; exit 77; }
systemd-detect-virt -q || { echo "SKIP: not in a VM"; exit 77; }
id -u "$U" >/dev/null 2>&1 || { echo "SKIP: no $U"; exit 77; }
EXP=$(pgrep -u "$U" -f 'explorer.exe' | head -1)
[ -n "$EXP" ] || { echo "SKIP: $U has no session"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sc-gate.XXXXXX); chmod 755 "$T"
PROBE=${SG_SC_PROBE:-}
if [ -z "$PROBE" ] && command -v x86_64-w64-mingw32-gcc >/dev/null; then
    x86_64-w64-mingw32-gcc -O1 -o "$T/p.exe" "$HERE/sessionchange-probe.c" -ladvapi32 && PROBE=$T/p.exe
fi
[ -n "$PROBE" ] || PROBE=$HERE/sessionchange-probe.exe
[ -f "$PROBE" ] || { echo "SKIP: no sessionchange-probe.exe"; rm -rf "$T"; exit 77; }
[ "$PROBE" = "$T/p.exe" ] || cp "$PROBE" "$T/p.exe"
chmod 755 "$T/p.exe"
touch "$T/log"; chmod 666 "$T/log"
SVC=sgsc_gate_$$
WLPID=
as_user() {
    # shellcheck disable=SC2046
    runuser -u "$U" -- env $(tr '\0' '\n' < /proc/"$EXP"/environ | grep -E '^(DISPLAY|WAYLAND_DISPLAY|XDG_RUNTIME_DIR|XAUTHORITY|HOME|USER|PATH|WINE[A-Z]*|SG_[A-Z_]*)=' | grep -v '^WINESERVERSOCKET=' | tr '\n' ' ') \
        WINEDEBUG=-all "$@"
}
# as a Windows service would run: SYSTEM, in the services' control group
as_service() {
    sh -c 'echo $$ > /sys/fs/cgroup/system.slice/sg-wineserver.service/cgroup.procs && exec runuser -u "$0" -- sh -c ". /usr/lib/stained-glass/sg-common.sh; sg_wine_env; export WINEDEBUG=-all; exec \"\$@\"" sh "$@"' "$SYS" "$@"
}
cleanup() {
    [ -n "$WLPID" ] && kill "$WLPID" 2>/dev/null
    as_service timeout 60 wine sc stop "$SVC" >/dev/null 2>&1
    as_service timeout 60 wine sc delete "$SVC" >/dev/null 2>&1
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
v() { printf '%s\n' "$1" | tr -d '\r' | sed -n "s/^$2 //p" | head -1; }
# a winlogon.exe for a uid's session, as sg-lockd starts it; its input is a fifo we hold
winlogon_for() {
    rm -f "$T/in"; mkfifo "$T/in"
    (exec 3>"$T/in"; sleep 600) &
    WLHOLD=$!
    SG_SESSION_UID=$1 as_service wine winlogon.exe < "$T/in" >/dev/null 2>&1 &
    WLPID=$!
}

S=$(v "$(cd "$T" && as_user timeout 120 wine "Z:$T/p.exe" session)" SESSION)
[ -n "$S" ] || { echo "FAIL  cannot learn $U's session"; exit 1; }
as_service timeout 60 wine sc create "$SVC" binPath= "Z:$T/p.exe --service Z:$T/log" >/dev/null 2>&1
as_service timeout 60 wine sc start "$SVC" >/dev/null 2>&1
nap 3
as_service timeout 60 wine sc query "$SVC" 2>/dev/null | grep -q RUNNING || { echo "FAIL  the test service does not run"; exit 1; }

# ---- 1. the person's session: connect, logon; logoff when it ends
winlogon_for "$(id -u "$U")"
nap 6
log=$(tr -d '\r' < "$T/log")
printf '%s\n' "$log" | grep -qx "EVENT 1 $S 8" && printf '%s\n' "$log" | grep -qx "EVENT 5 $S 8" \
    && pass "a session's winlogon.exe: the service is told of the console connect and the logon of session $S" \
    || fail "no logon event for session $S: [$log]"
kill "$WLHOLD" 2>/dev/null; wait "$WLPID" 2>/dev/null; WLPID=
nap 4
tr -d '\r' < "$T/log" | grep -qx "EVENT 6 $S 8" && pass "...and of the logoff when the session ends" \
    || fail "no logoff event: [$(tr -d '\r' < "$T/log")]"

# ---- 2. the sign-in screen's session (a system account) is no logon
GU=$(id -u "${SG_GREETER_USER:-sggreet}" 2>/dev/null)
if [ -n "$GU" ] && [ "$GU" -lt 1000 ]; then
    : > "$T/log"
    winlogon_for "$GU"
    nap 6
    kill "$WLHOLD" 2>/dev/null; wait "$WLPID" 2>/dev/null; WLPID=
    nap 3
    [ -z "$(tr -d '\r' < "$T/log")" ] && pass "the sign-in screen's session (uid $GU) is no logon: the service is told nothing" \
        || fail "the sign-in screen's session: [$(tr -d '\r' < "$T/log")]"
else
    echo "      (no sign-in screen account: skipped)"
fi

# ---- 3. a person's own program cannot send them
: > "$T/log"
o=$(cd "$T" && as_user timeout 120 wine "Z:$T/p.exe" tell 5 "$S")
nap 3
[ "$(v "$o" TELL)" = 5 ] && [ -z "$(tr -d '\r' < "$T/log")" ] && pass "a person's own program cannot send session events (access denied)" \
    || fail "a person's program: TELL $(v "$o" TELL), log [$(tr -d '\r' < "$T/log")]"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
