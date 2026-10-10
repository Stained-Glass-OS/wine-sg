#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# LogonUser in a shared prefix (patches/sg/1709): as in a real installation,
# the prefix belongs to the SYSTEM account (SG_SYSTEM, default sgsystem), two
# standard users (SG_USER1, SG_USER2, default sgtest1 and sgtest2, in
# SG_GROUP, default sgwine) run programs against its one wineserver, and the
# elevation broker (sg-session's sg-brokerd, built from SG_SESSION_SRC,
# default ../sg-session) runs as root and checks passwords -- here with a
# stand-in for PAM that takes one made-up password for any account
# (test/logonuser-probe.c):
#  1. SG_USER1 logs on as SG_USER2 with the password: a primary token for
#     SG_USER2 in SG_USER1's session, with a logon SID; impersonating it,
#     GetUserName is SG_USER2; a network logon gives an impersonation token
#  2. a wrong password, an unknown account, root and the SYSTEM account:
#     ERROR_LOGON_FAILURE
#  3. the ticket the wineserver takes: once only; not one owned by anybody
#     but the SYSTEM account, not one made for another requester, not an
#     expired one, never one for SYSTEM
#  4. the password is in no log and no ticket
# Exit 77 without the accounts, passwordless sudo or the broker's source.
# Mutants: SG_MUTANT_TICKET_ANY_OWNER, SG_MUTANT_TICKET_ANY_REQUESTER
# (server/token.c); sg-session's SG_MUTANT_LOGON_NO_PASSWORD_CHECK.
#   WINE=... WINESERVER=... [SG_SESSION_SRC=...] test/logonuser-gate.sh
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
[ -f "$SG_SESSION_SRC/broker/sg-brokerd.c" ] && grep -q '"@logon"' "$SG_SESSION_SRC/broker/sg-brokerd.c" \
    || { echo "SKIP: no sg-brokerd with @logon in SG_SESSION_SRC ($SG_SESSION_SRC)"; exit 77; }
W=$(mktemp -d /var/tmp/logonuser.XXXXXX)
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

"$MINGW" -O1 -o "$W/logonuser-probe.exe" "$HERE/logonuser-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/logonuser-probe.exe"
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
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/logonuser-probe.exe" "$PFX/drive_c/logonuser-probe.exe"

# the broker, as root, with a stand-in for PAM: one made-up password (for this
# run only) for any account
PW="t$(od -An -N6 -tx1 /dev/urandom | tr -d ' \n')"
gcc -O2 -o "$W/sg-brokerd" "$SG_SESSION_SRC/broker/sg-brokerd.c" || { echo "FAIL build sg-brokerd"; exit 1; }
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
    SG_AUDIT_SPOOL="$W/audit" "$W/sg-brokerd" & BROKER_PID=$!
for i in $(seq 50); do [ -S "$W/run/broker.sock" ] && break; sleep 0.2; done
[ -S "$W/run/broker.sock" ] || { echo "FAIL: the broker did not start"; sudo -n cat "$W/brokerd.log"; exit 1; }
BROKER_PID=$(sudo -n pgrep -f "^$W/sg-brokerd" | head -1)

probe() { local u=$1; shift; as "$u" timeout -s KILL 90 "$WINE" 'C:\logonuser-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
for u in "$SG_USER1" "$SG_USER2"; do probe "$u" me >/dev/null; done
sid2=$(v "$(probe "$SG_USER2" me)" ME)
# the password on the probe's standard input: never on a command line, which
# the wineserver keeps
pwprobe() { local u=$1 pw=$2; shift 2; printf '%s\n' "$pw" | as "$u" timeout -s KILL 90 "$WINE" 'C:\logonuser-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
out=$(pwprobe "$SG_USER1" "$PW" logon "$SG_USER2" - 2)
q=$(v "$out" LOGON)
case "$q" in
    "1 $sid2 "[1-9]*" primary") pass "$SG_USER1 logs on as $SG_USER2 with the password: $q";;
    *) fail "logon with the password: $q (want $sid2)";;
esac
[ "$(v "$out" LOGONSID)" = 1 ] && pass "...with a logon SID" || fail "no logon SID"
[ "$(v "$out" IMPERSONATE)" = "$SG_USER2" ] && pass "...impersonating it, GetUserName is $SG_USER2" || fail "impersonating: $(v "$out" IMPERSONATE)"
q=$(v "$(pwprobe "$SG_USER1" "$PW" logon "$SG_USER2" - 3)" LOGON)
case "$q" in "1 $sid2 "*" impersonation") pass "a network logon gives an impersonation token";; *) fail "network logon: $q";; esac
q=$(v "$(pwprobe "$SG_USER1" "x$PW" logon "$SG_USER2" - 2)" LOGON)
[ "$q" = "0 1326" ] && pass "a wrong password: ERROR_LOGON_FAILURE" || fail "wrong password: $q"
for who in sgnosuchuser root "$SG_SYSTEM"; do
    q=$(v "$(pwprobe "$SG_USER1" "$PW" logon "$who" - 2)" LOGON)
    [ "$q" = "0 1326" ] && pass "$who, even with the password: ERROR_LOGON_FAILURE" || fail "$who: $q"
done

# tickets the wineserver is offered by name
uid1=$(id -u "$SG_USER1"); uid2=$(id -u "$SG_USER2"); uids=$(id -u "$SG_SYSTEM")
plant() { # NAME OWNER UID FOR EXPIRES [MODE]
    printf 'uid=%s\nfor=%s\ntype=2\nexpires=%s\n' "$3" "$4" "$5" | sudo -n tee "$W/run/tickets/$1" >/dev/null
    sudo -n chown "$2" "$W/run/tickets/$1"; sudo -n chmod "${6:-600}" "$W/run/tickets/$1"
}
redeem() { as "$SG_USER1" env SG_LOGON_TICKET="$1" timeout -s KILL 90 "$WINE" 'C:\logonuser-probe.exe' logon "$SG_USER2" x 2 2>/dev/null | tr -d '\r'; }
now=$(date +%s)
plant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1 "$SG_SYSTEM" "$uid2" "$uid1" $((now + 60))
q=$(v "$(redeem aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1)" LOGON)
case "$q" in "1 $sid2 "*) pass "a ticket the broker would make is taken";; *) fail "a good ticket: $q";; esac
[ "$(v "$(redeem aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1)" LOGON)" = "0 1326" ] && pass "...once only" || fail "a ticket taken twice"
# (one the server could read: a user's file that anyone may read)
plant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2 "$SG_USER1" "$uid2" "$uid1" $((now + 60)) 644
[ "$(v "$(redeem aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2)" LOGON)" = "0 1326" ] && pass "not one owned by $SG_USER1" || fail "a ticket owned by $SG_USER1 was taken"
plant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3 "$SG_SYSTEM" "$uid2" "$uid2" $((now + 60))
[ "$(v "$(redeem aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3)" LOGON)" = "0 1326" ] && pass "not one made for $SG_USER2" || fail "another requester's ticket was taken"
plant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4 "$SG_SYSTEM" "$uid2" "$uid1" $((now - 5))
[ "$(v "$(redeem aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4)" LOGON)" = "0 1326" ] && pass "not an expired one" || fail "an expired ticket was taken"
plant aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa5 "$SG_SYSTEM" "$uids" "$uid1" $((now + 60))
[ "$(v "$(redeem aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa5)" LOGON)" = "0 1326" ] && pass "never one for the SYSTEM account" || fail "a ticket for SYSTEM was taken"

# the password: in no log, no ticket, nothing the wineserver keeps (its
# processes' command lines and environments are in its debug dump)
if sudo -n sh -c 'grep -rqF "$1" "$2/brokerd.log" "$2/run/tickets" "$2/audit" "$3"/*.reg' sh "$PW" "$W" "$PFX" 2>/dev/null; then fail "the password is in a log, a ticket or the registry"
else pass "the password is in no log, no ticket and not in the registry"; fi

sudo -n kill "$BROKER_PID" 2>/dev/null; BROKER_PID=
as "$SG_USER1" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
