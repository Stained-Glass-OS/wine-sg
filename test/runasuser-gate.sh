#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Running a program as another account in a shared prefix (patches/sg/1711):
# as in a real installation, the prefix belongs to the SYSTEM account
# (SG_SYSTEM, default sgsystem), two standard users (SG_USER1, SG_USER2,
# default sgtest1 and sgtest2, in SG_GROUP, default sgwine) run programs
# against its one wineserver, and the elevation broker (sg-session's
# sg-brokerd, built from SG_SESSION_SRC, default ../sg-session) runs as root,
# here with a stand-in for PAM that takes one made-up password for any
# account (test/runasuser-probe.c):
#  1. CreateProcessWithLogonW and CreateProcessWithTokenW by SG_USER1, with
#     SG_USER2's password: the program runs as SG_USER2 -- its Windows SID,
#     and the file it writes is SG_USER2's on disk -- in SG_USER2's own
#     environment (its profile), with nothing of SG_USER1's (a variable it set);
#     the caller has its process and waits for its exit code
#  2. a wrong password starts nothing (ERROR_LOGON_FAILURE)
#  3. SYSTEM (a service) starts a program on a logged-on user's session with
#     WTSQueryUserToken's token and CreateProcessAsUser
#  4. the broker starts nothing for a launch ticket made for another
#     requester, nor as a system account (below 1000)
# Exit 77 without the accounts, passwordless sudo or the broker's source.
# Mutants: SG_MUTANT_LAUNCH_ANY_REQUESTER, SG_MUTANT_SPAWN_SYSTEM_ACCOUNTS
# (sg-session's broker/sg-brokerd.c, built with SG_BROKER_CFLAGS).
#   WINE=... WINESERVER=... [SG_SESSION_SRC=...] test/runasuser-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}
SG_USER1=${SG_USER1:-sgtest1}
SG_USER2=${SG_USER2:-sgtest2}
SG_GROUP=${SG_GROUP:-sgwine}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
SG_SESSION_SRC=${SG_SESSION_SRC:-$HERE/../../sg-session}
[ -f "$SG_SESSION_SRC/broker/sg-brokerd.c" ] && grep -q '"@launch"' "$SG_SESSION_SRC/broker/sg-brokerd.c" \
    || { echo "SKIP: no sg-brokerd with @launch in SG_SESSION_SRC ($SG_SESSION_SRC)"; exit 77; }
W=$(mktemp -d /var/tmp/runasuser.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
# SYSTEM's programs live in the service window station, as sg-wineserver and
# sg-services-start run them: the interactive WinSta0 is the users'
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        SG_BROKER_SOCK="$W/run/broker.sock" SG_LOGON_TICKET_DIR="$W/run/tickets" \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" "$@"
}
cleanup() {
    set +e
    [ -n "${BROKER_PID:-}" ] && sudo -n kill "$BROKER_PID" 2>/dev/null
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O1 -o "$W/runasuser-probe.exe" "$HERE/runasuser-probe.c" -ladvapi32 -lwtsapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/runasuser-probe.exe"
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
# the machine server restarted as at boot (its first process on the services'
# window station, no display recorded), and the display containers
# sg-session's sg_protect_machine_registry makes at every boot, beneath which
# each user's desktop records its display (as test/dispcfg-gate.sh does)
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
for k in 'HKLM\System\CurrentControlSet\Control\Video' 'HKLM\System\CurrentControlSet\Control\GraphicsDrivers'; do
    as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 120 '$WINE' reg add '$k' /f" >/dev/null 2>&1
done
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
# no display here: the null graphics driver, so windows can be made
for u in "$SG_USER1" "$SG_USER2"; do
    as "$u" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
done
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/runasuser-probe.exe" "$PFX/drive_c/runasuser-probe.exe"

# the broker, as root, with a stand-in for PAM: one made-up password (for this
# run only) for any account
PW="t$(od -An -N6 -tx1 /dev/urandom | tr -d ' \n')"
gcc -O2 ${SG_BROKER_CFLAGS:-} -o "$W/sg-brokerd" "$SG_SESSION_SRC/broker/sg-brokerd.c" || { echo "FAIL build sg-brokerd"; exit 1; }
cat > "$W/pamcheck" <<EOS
#!/bin/bash
IFS= read -r -d '' user; IFS= read -r -d '' pass
[ "\$pass" = "$PW" ] && echo OK && exit 0
echo FAIL; exit 1
EOS
sudo -n install -d -m 755 "$W/run" && sudo -n install -d -m 700 -o "$SG_SYSTEM" "$W/audit"
sudo -n chown root:root "$W/sg-brokerd" "$W/pamcheck" && sudo -n chmod 755 "$W/sg-brokerd" "$W/pamcheck"
sudo -n env SG_BROKER_FOREGROUND=1 SG_BROKER_SOCK="$W/run/broker.sock" SG_BROKER_PAMCHECK="$W/pamcheck" \
    SG_SYSTEM_USER="$SG_SYSTEM" SG_LOGON_TICKET_DIR="$W/run/tickets" SG_BROKERD_LOG="$W/brokerd.log" \
    SG_AUDIT_SPOOL="$W/audit" SG_LAUNCH_WINE="$WINE" SG_LAUNCH_PREFIX="$PFX" "$W/sg-brokerd" & BROKER_PID=$!
for i in $(seq 50); do [ -S "$W/run/broker.sock" ] && break; sleep 0.2; done
[ -S "$W/run/broker.sock" ] || { echo "FAIL: the broker did not start"; sudo -n cat "$W/brokerd.log"; exit 1; }
BROKER_PID=$(sudo -n pgrep -f "^$W/sg-brokerd" | head -1)


probe() { local u=$1; shift; as "$u" timeout -s KILL 150 "$WINE" 'C:\runasuser-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
pwprobe() { local u=$1 pw=$2; shift 2; printf '%s\n' "$pw" | as "$u" env SG_PROBE_SECRET=from-the-requester timeout -s KILL 150 "$WINE" 'C:\runasuser-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
report() { sudo -n cat "$PFX/drive_c/out/$1" 2>/dev/null | tr -d '\r'; }
owner() { sudo -n stat -c %U "$PFX/drive_c/out/$1" 2>/dev/null; }
sudo -n install -d -m 1777 -o "$SG_SYSTEM" "$PFX/drive_c/out"
sid2=$(as "$SG_USER2" timeout -s KILL 90 "$WINE" cmd /c whoami /user 2>/dev/null | tr -d '\r' | grep -o 'S-1-5-21-[0-9-]*' | head -1)

# each user's first program sets the user up: done first
for u in "$SG_USER1" "$SG_USER2"; do as "$u" timeout -s KILL 300 "$WINE" 'C:\runasuser-probe.exe' report "C:\\out\\warm-$u" >/dev/null 2>&1; done

for how in withlogon withtoken; do
    out=$(pwprobe "$SG_USER1" "$PW" $how "$SG_USER2" "C:\\out\\$how")
    r=$(report $how)
    [ "$(v "$out" STARTED | cut -d' ' -f1)" = 1 ] && [ "$(v "$out" EXIT)" = 7 ] \
        && pass "$how: $SG_USER1 starts a program as $SG_USER2 and has its exit code" || fail "$how: $out"
    [ -n "$sid2" ] && [ "$(v "$r" SID)" = "$sid2" ] && [ "$(owner $how)" = "$SG_USER2" ] \
        && pass "...it is $SG_USER2 on Windows ($sid2) and on disk" || fail "$how: SID $(v "$r" SID) (want $sid2), file owner $(owner $how)"
    [ "$(v "$r" USERNAME)" = "$SG_USER2" ] && [ "$(v "$r" PROFILE)" = "C:\\users\\$SG_USER2" ] && [ "$(v "$r" SECRET)" = "-" ] \
        && pass "...with $SG_USER2's own environment (profile), nothing of $SG_USER1's" \
        || fail "$how: USERNAME $(v "$r" USERNAME), profile $(v "$r" PROFILE), secret $(v "$r" SECRET)"
done
out=$(pwprobe "$SG_USER1" "x$PW" withlogon "$SG_USER2" 'C:\out\bad')
[ "$(v "$out" STARTED)" = "0 1326" ] && [ -z "$(report bad)" ] && pass "a wrong password starts nothing (ERROR_LOGON_FAILURE)" \
    || fail "wrong password: $out"

# a service: SYSTEM starts a program on a logged-on user's session
as "$SG_USER2" timeout -s KILL 300 "$WINE" 'C:\runasuser-probe.exe' report 'C:\out\hold' >/dev/null 2>&1
as "$SG_USER2" timeout -s KILL 300 "$WINE" cmd /c 'ping -n 200 127.0.0.1 >nul' >/dev/null 2>&1 &
sleep 5
s2=$(v "$(report hold)" SESSION)
out=$(probe "$SG_SYSTEM" asuser "$s2" 'C:\out\asuser')
r=$(report asuser)
[ "$(v "$out" STARTED | cut -d' ' -f1)" = 1 ] && [ "$(v "$out" EXIT)" = 7 ] && [ "$(v "$r" SID)" = "$sid2" ] \
    && [ "$(v "$r" SESSION)" = "$s2" ] && [ "$(owner asuser)" = "$SG_USER2" ] \
    && pass "SYSTEM starts a program as $SG_USER2 in its session $s2 (WTSQueryUserToken, CreateProcessAsUser)" \
    || fail "asuser: $out / $r / owner $(owner asuser)"

# the broker itself: a launch ticket for another requester; a system account
cat > "$W/client.py" <<'PY'
import socket, struct, sys
blob = b'\0'.join([b'@launch', b'LAUNCH_TICKET=' + sys.argv[2].encode(), b'LAUNCH_CWD=/', b'',
                   b'C:\\runasuser-probe.exe', b'report', sys.argv[3].encode()]) + b'\0'
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); s.sendall(struct.pack('=I', len(blob)) + blob)
print('STATUS', s.recv(1)[0])
PY
uid1=$(id -u "$SG_USER1"); uid2=$(id -u "$SG_USER2")
plant() { # NAME UID FOR
    printf 'kind=launch\nuid=%s\nfor=%s\nexpires=%s\n' "$2" "$3" $(( $(date +%s) + 60 )) | sudo -n tee "$W/run/tickets/$1" >/dev/null
    sudo -n chown "$SG_SYSTEM" "$W/run/tickets/$1"; sudo -n chmod 600 "$W/run/tickets/$1"
}
raw() { sudo -n -u "$SG_USER1" python3 "$W/client.py" "$W/run/broker.sock" "$1" "$2" 2>/dev/null; }
plant bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1 "$uid2" "$uid2"
[ "$(raw bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1 'C:\out\stolen')" = "STATUS 1" ] && sleep 3 && [ -z "$(report stolen)" ] \
    && pass "the broker starts nothing for a ticket made for another requester" || fail "another requester's ticket started something"
plant bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2 "$(id -u daemon)" "$uid1"
[ "$(raw bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2 'C:\out\daemon')" = "STATUS 1" ] && sleep 3 && [ -z "$(report daemon)" ] \
    && pass "...nor as a system account" || fail "a program was started as a system account"

sudo -n kill "$BROKER_PID" 2>/dev/null; BROKER_PID=
as "$SG_USER1" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
