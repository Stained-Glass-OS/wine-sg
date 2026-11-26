#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# SetTokenInformation(TokenSessionId) and a service's helper in a person's
# session (patches/sg/9624, 9625; ADR 0010 pattern A). As in a real
# installation the prefix belongs to the SYSTEM account (SG_SYSTEM, default
# sgsystem) and standard users (SG_USER1, SG_USER2) run programs against its
# one wineserver, each in a session of its own (1706)
# (test/tokensession-probe.c):
#  1. SYSTEM (SeTcbPrivilege) gives a copy of its token a user's session, as a
#     remote-support service does for its helper; a session nobody is on is
#     ERROR_INVALID_PARAMETER
#  2. a standard user cannot (ERROR_PRIVILEGE_NOT_HELD), for another user's
#     session or its own, and the token keeps its session (the stub before
#     said yes to anyone)
#  3. CreateProcessAsUser with SYSTEM's copy: the program is SYSTEM (S-1-5-18)
#     in the user's session, started through sg-elevated-run --system for the
#     user's compositor (a stand-in here, SG_ELEVATED_RUN, on a private seat
#     directory, SG_SEAT_ROOT), which tells it the privileged socket
#  4. without a compositor for that user it still starts there, without
#  5. a standard user's CreateProcessAsUser with its own token is as before
# Exit 77 without the accounts or passwordless sudo.
# Mutant: SG_MUTANT_TOKEN_SESSION_ANYONE (server/token.c).
#   WINE=... WINESERVER=... test/tokensession-gate.sh
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
W=$(mktemp -d /var/tmp/tokensession.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
# SYSTEM's programs live in the service window station, as sg-wineserver and
# sg-services-start run them: the interactive WinSta0 is the users'
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" "$@"
}
cleanup() {
    set +e
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O1 -o "$W/tokensession-probe.exe" "$HERE/tokensession-probe.c" -lwtsapi32 -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/tokensession-probe.exe"
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
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/tokensession-probe.exe" "$PFX/drive_c/tokensession-probe.exe"
# a user's first program sets the user up (profile, desktop): done first,
# for both, so the timing below is that of programs, not of first logons
# (the first attempt may give up)
for u in "$SG_USER1" "$SG_USER2"; do
    for i in 1 2 3; do
        as "$u" timeout -s KILL 300 "$WINE" 'C:\tokensession-probe.exe' whoami >"$W/warm-$u.out" 2>/dev/null && break
    done
    echo "warm-up $u: $(tr -d '\r' < "$W/warm-$u.out" | grep SESSION)"
done
probe() { local u=$1; shift; as "$u" timeout -s KILL 120 env SG_ELEVATED_RUN="$W/standin" SG_SEAT_ROOT="$W/seat" "$WINE" 'C:\tokensession-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
# what the started program wrote, once (then gone, for the next)
who() { sudo -n cat "$PFX/drive_c/who.txt" 2>/dev/null | tr -d '\r'; sudo -n rm -f "$PFX/drive_c/who.txt"; }

# sg-elevated-run's stand-in: notes how it was called, then runs the program
# as sg-elevated-run --system would (SG_COMPOSITOR_PRIV beside --control)
cat > "$W/standin" <<SH
#!/bin/sh
echo "\$*" >> "$W/standin.log"
ctl=
while [ "\$1" != -- ]; do [ "\$1" = --control ] && ctl=\$2; shift; done
shift
echo "priv=\${ctl%/*}/priv.sock" >> "$W/standin.log"
SG_COMPOSITOR_PRIV="\${ctl%/*}/priv.sock" exec "\$@"
SH
chmod 755 "$W/standin"
: > "$W/standin.log"; chmod 666 "$W/standin.log"

as "$SG_USER1" timeout -s KILL 500 "$WINE" 'C:\tokensession-probe.exe' hold 400 > "$W/hold.out" 2>/dev/null &
for i in $(seq 240); do grep -q HELD "$W/hold.out" 2>/dev/null && break; sleep 1; done
s1=$(v "$(tr -d '\r' < "$W/hold.out")" SESSION)
[ -n "$s1" ] && [ "$s1" != 1 ] && pass "$SG_USER1 is logged on to session $s1" || fail "$SG_USER1's session: $s1"
u1=$(id -u "$SG_USER1")

# 1.
r=$(v "$(probe "$SG_SYSTEM" set "$s1")" SET)
[ "$r" = "1 $s1" ] && pass "SYSTEM gives its token's copy session $s1" || fail "SYSTEM, set $s1: $r"
r=$(v "$(probe "$SG_SYSTEM" set 77)" SET)
[ "$r" = "0 87 1" ] && pass "a session nobody is on: ERROR_INVALID_PARAMETER, the token keeps its own" || fail "SYSTEM, set 77: $r"
# 2.
r=$(v "$(probe "$SG_USER2" set "$s1")" SET)
case "$r" in "0 1314 "*) [ "$r" != "0 1314 $s1" ] && pass "$SG_USER2 cannot give a token $SG_USER1's session (ERROR_PRIVILEGE_NOT_HELD): $r" \
                               || fail "$SG_USER2 refused yet the token has $s1";;
             *) fail "$SG_USER2, set $s1: $r";; esac
r=$(v "$(probe "$SG_USER1" set 1)" SET)
case "$r" in "0 1314 $s1") pass "...nor $SG_USER1 its own token SYSTEM's session ($r)";; *) fail "$SG_USER1, set 1: $r";; esac

# 3. a compositor for the user (a socket in its seat directory)
install -d -m 755 "$W/seat" "$W/seat/seat0" "$W/seat/seat0/$u1"
python3 -c 'import socket,sys; s=socket.socket(socket.AF_UNIX); s.bind(sys.argv[1])' "$W/seat/seat0/$u1/control.sock"
chmod 777 "$W/seat/seat0/$u1/control.sock"
r=$(probe "$SG_SYSTEM" spawn "$s1" 'C:\tokensession-probe.exe whoami C:\who.txt')
sp=$(v "$r" SPAWN)
[ "$sp" = "1 0" ] && pass "SYSTEM starts a program with that token (CreateProcessAsUser)" || fail "spawn: $r"
log=$(cat "$W/standin.log")
case "$(head -1 "$W/standin.log")" in
    "--system --control $W/seat/seat0/$u1/control.sock --uid $u1 -- "*) pass "...through sg-elevated-run --system for $SG_USER1's compositor";;
    *) fail "the wrapper was called: '$log'";;
esac
who=$(v "$(who)" WHO)
[ "$who" = "$s1 S-1-5-18 -" ] && pass "...and it is SYSTEM in session $s1: $who" || fail "the helper: '$who'"
grep -qx "priv=$W/seat/seat0/$u1/priv.sock" "$W/standin.log" && pass "...told the privileged socket beside the control socket" \
    || fail "the stand-in's log: $(cat "$W/standin.log")"
# 4. no compositor: started all the same, without the wrapper
rm -f "$W/seat/seat0/$u1/control.sock"; : > "$W/standin.log"
r=$(probe "$SG_SYSTEM" spawn "$s1" 'C:\tokensession-probe.exe whoami C:\who.txt')
[ "$(v "$(who)" WHO)" = "$s1 S-1-5-18 -" ] && [ ! -s "$W/standin.log" ] && pass "without a compositor for $SG_USER1 the helper still starts in session $s1, without a display" \
    || fail "no compositor: '$r' / '$(cat "$W/standin.log")'"
# 5.
: > "$W/standin.log"
r=$(probe "$SG_USER1" spawn "$s1" 'C:\tokensession-probe.exe whoami C:\who.txt')
who >/dev/null
case "$(v "$r" SPAWN)" in "0 1314") pass "$SG_USER1's own spawn: its set is refused first (ERROR_PRIVILEGE_NOT_HELD)";; *) fail "$SG_USER1 spawn: $r";; esac
s0=$(v "$(probe "$SG_SYSTEM" whoami)" WHO)
r=$(probe "$SG_SYSTEM" spawn 1 'C:\tokensession-probe.exe whoami C:\who.txt')
case "$(v "$(who)" WHO)" in "1 S-1-5-18 -") [ ! -s "$W/standin.log" ] && pass "SYSTEM's own session: started as ever ($s0)" || fail "SYSTEM's own session went through the wrapper";;
    *) fail "SYSTEM, its own session: $r";; esac

as "$SG_USER1" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
