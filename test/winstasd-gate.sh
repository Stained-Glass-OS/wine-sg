#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Window station and desktop security in a shared prefix (patches/sg/1707):
# as in a real installation, the prefix belongs to the SYSTEM account
# (SG_SYSTEM, default sgsystem) and two standard users (SG_USER1, SG_USER2,
# default sgtest1 and sgtest2, all in SG_GROUP, default sgwine) run programs
# against its one wineserver at once, each in a session of its own (1706)
# (test/winstasd-probe.c):
#  1. a user's WinSta0 and desktop belong to the user: owner the user, full
#     access for the user and SYSTEM, read for Administrators, nobody else
#  2. the other user cannot open the first's WinSta0 by its \Sessions path,
#     not even to read it, nor for its clipboard
#  3. SYSTEM can; the user's own second process can
#  4. SetSecurityInfo(SE_WINDOW_OBJECT) changes a desktop's DACL
#  5. users no longer share SYSTEM's session
# Exit 77 without the accounts or passwordless sudo.
# Mutants: SG_MUTANT_OPEN_WINSTATIONS (server/winstation.c),
#   SG_MUTANT_SETSECINFO_WINDOW_NOOP (dlls/advapi32/security.c).
#   WINE=... WINESERVER=... test/winstasd-gate.sh
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
W=$(mktemp -d /var/tmp/winstasd.XXXXXX)
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

"$MINGW" -O1 -o "$W/winstasd-probe.exe" "$HERE/winstasd-probe.c" -luser32 -ladvapi32 -lntdll || { echo "FAIL build"; exit 1; }
chmod 755 "$W/winstasd-probe.exe"
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
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/winstasd-probe.exe" "$PFX/drive_c/winstasd-probe.exe"
# a user's first program sets the user up (profile, desktop): done first,
# for both, so the timing below is that of programs, not of first logons
# (a window, so its desktop and explorer too; the first attempt may give up)
for u in "$SG_USER1" "$SG_USER2"; do
    for i in 1 2 3; do
        as "$u" timeout -s KILL 300 "$WINE" 'C:\winstasd-probe.exe' win >"$W/warm-$u.out" 2>/dev/null && break
    done
    echo "warm-up $u: $(tr -d '\r' < "$W/warm-$u.out" | grep WIN)"
done
probe() { local u=$1; shift; as "$u" timeout -s KILL 90 "$WINE" 'C:\winstasd-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }

as "$SG_USER1" timeout -s KILL 400 "$WINE" 'C:\winstasd-probe.exe' hold > "$W/hold.out" 2>/dev/null &
for i in $(seq 240); do grep -q HELD "$W/hold.out" 2>/dev/null && break; sleep 1; done
held=$(tr -d '\r' < "$W/hold.out")
s1=$(v "$held" SESSION)
[ "$(v "$held" HELD)" = 1 ] && pass "$SG_USER1 has a window in session $s1" || fail "$SG_USER1 hold: $held"
[ -n "$s1" ] && [ "$s1" != 1 ] && pass "...not in SYSTEM's session 1" || fail "$SG_USER1 is in session $s1"

out=$(probe "$SG_USER1" own)
u1=$(v "$out" USER)
[ -n "$u1" ] && printf '%s\n' "$out" | grep -q "^WINSTA OWNER $u1\$" && printf '%s\n' "$out" | grep -q "^DESKTOP OWNER $u1\$" \
    && pass "$SG_USER1 owns its window station and desktop" || fail "owners: $(printf '%s\n' "$out" | grep OWNER | tr '\n' ' ')"
for o in WINSTA DESKTOP; do
    aces=$(printf '%s\n' "$out" | sed -n "s/^$o ACE 0 //p" | awk '{print $1}' | sort | tr '\n' ' ')
    [ "$aces" = "$(printf '%s\n' S-1-5-18 S-1-5-32-544 "$u1" | sort | tr '\n' ' ')" ] \
        && pass "$o: SYSTEM, Administrators and $SG_USER1 only" || fail "$o ACEs: $aces"
done
printf '%s\n' "$out" | grep -q "^WINSTA ACE 0 S-1-5-32-544 0x20303$" && printf '%s\n' "$out" | grep -q "^DESKTOP ACE 0 S-1-5-32-544 0x20041$" \
    && pass "...Administrators may only read" || fail "Administrators: $(printf '%s\n' "$out" | grep S-1-5-32-544 | tr '\n' ' ')"
[ "$(v "$out" SETSEC)" = 0 ] && printf '%s\n' "$out" | grep -q "^AFTER ACE 0 S-1-1-0 " \
    && pass "SetSecurityInfo(SE_WINDOW_OBJECT) changes the desktop's DACL" || fail "SetSecurityInfo: $(v "$out" SETSEC), $(printf '%s\n' "$out" | grep '^AFTER ACE' | tr '\n' ' ')"

out=$(probe "$SG_USER2" open "$s1")
s2=$(v "$out" SESSION)
[ -n "$s2" ] && [ "$s2" != "$s1" ] && [ "$s2" != 1 ] && pass "$SG_USER2 has session $s2" || fail "$SG_USER2's session: $s2"
[ "$(v "$out" 'OPEN READ')" = "0 5" ] && [ "$(v "$out" 'OPEN CLIPBOARD')" = "0 5" ] \
    && pass "$SG_USER2 cannot open $SG_USER1's WinSta0, to read it or for its clipboard (access denied)" \
    || fail "$SG_USER2 opening $SG_USER1's WinSta0: $(printf '%s\n' "$out" | grep OPEN | tr '\n' ' ')"
out=$(probe "$SG_USER1" open "$s1")
[ "$(v "$out" 'OPEN READ')" = "1 0" ] && [ "$(v "$out" 'OPEN CLIPBOARD')" = "1 0" ] \
    && pass "$SG_USER1's own process can" || fail "$SG_USER1 opening its WinSta0: $(printf '%s\n' "$out" | grep OPEN | tr '\n' ' ')"
out=$(probe "$SG_SYSTEM" open "$s1")
[ "$(v "$out" 'OPEN READ')" = "1 0" ] && [ "$(v "$out" 'OPEN CLIPBOARD')" = "1 0" ] \
    && pass "SYSTEM can" || fail "SYSTEM opening $SG_USER1's WinSta0: $(printf '%s\n' "$out" | grep OPEN | tr '\n' ' ')"

as "$SG_USER1" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
