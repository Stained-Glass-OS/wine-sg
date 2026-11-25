#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Capped (2026-11-24: an earlier draft of this gate, on a Wine whose sandboxed desktop could not be
# made, left 13,370 explorer.exe processes that filled the host's memory and swap): the gate runs in
# a scope of its own -- MemoryMax 2G, TasksMax 400, at most 20 minutes -- and on exit, or when
# killed, every process in it goes, its wineserver first.
if [ -z "${SG_GATE_SCOPED:-}" ]; then
    # root (a QA VM) makes its own scope; anyone else runs it through make (test-logondesk*), whose
    # rule wraps it in a user scope with the same limits
    [ "$(id -u)" = 0 ] && systemd-run --scope -q -p MemoryMax=2G -p TasksMax=400 -p RuntimeMaxSec=1200 -- true 2>/dev/null ||
        { echo "SKIP: needs a capped scope: run it as make test-$(basename "$0" -gate.sh) (or as root)"; exit 77; }
    export SG_GATE_SCOPED=1
    exec systemd-run --scope -q -p MemoryMax=2G -p TasksMax=400 -p RuntimeMaxSec=1200 -- "$(cd "$(dirname "$0")" && pwd)/$(basename "$0")" "$@"
fi
case "$(sed -n 's/^0:://p' /proc/self/cgroup 2>/dev/null)" in
*.scope) ;; *) echo "SKIP: not in a capped scope (make test-$(basename "$0" -gate.sh))"; exit 77 ;; esac
sg_gate_cgroup_kill() {
    c=$(sed -n 's/^0:://p' /proc/self/cgroup 2>/dev/null)
    case "$c" in *.scope) ;; *) return 0 ;; esac
    [ -n "${SG_GATE_SCOPED:-}" ] && [ -r "/sys/fs/cgroup$c/cgroup.procs" ] || return 0
    for _r in 1 2 3; do
        for _p in $(cat "/sys/fs/cgroup$c/cgroup.procs"); do [ "$_p" = $$ ] || kill -9 "$_p" 2>/dev/null; done
    done
}
# Logon SIDs and the desktop's descriptor in a shared prefix (patches/sg/9060): as in a real
# installation, the prefix belongs to the SYSTEM account (SG_SYSTEM, default sgsystem) and two
# standard users (SG_USER1, SG_USER2, default sgtest1 and sgtest2, all in SG_GROUP, default sgwine)
# run programs against its one wineserver, each in a session of its own. On Windows each logon has
# a logon SID of its own, and winlogon's descriptor on its WinSta0 and desktops names it: that is
# what a sandboxed process (Chromium's USER_LIMITED GPU, audio and utility processes), whose
# restricting SIDs are Users, Everyone, RESTRICTED and the logon SID, is let onto the desktop by.
# Wine gave every token the same S-1-5-5-0-0, so no descriptor could name it without opening every
# user's desktop to every other user. The probe (test/logondesk-probe.c):
#  1. each user's token has a logon SID, S-1-5-5-X-Y with X not 0, and the two users' differ
#  2. the user's WinSta0 and desktop name the user's own logon SID with full access, and not the
#     other user's; SYSTEM, Administrators (read) and the user as before (1707)
#  3. a USER_LIMITED child of the user is connected to the user's desktop, makes windows, and may
#     open the desktop itself (by the logon SID: its restricting SIDs do not name the user)
#  4. the other user cannot open the first's WinSta0, to read it or for its clipboard, nor can the
#     other user's USER_LIMITED child; the user's own USER_LIMITED child and SYSTEM can
#  5. the logon SID stays the user's: the user's next process has the same
# (Chromium's alternate desktop is test/logondesk-gate.sh's: here the null driver gives a sandboxed
# desktop's own explorer no display to record.)
# Exit 77 without the accounts or passwordless sudo.
# Mutants: SG_MUTANT_SHARED_LOGON_SID (server/token.c: everybody's logon SID S-1-5-5-0-0, as
#   upstream, and the ACE naming it -- clause 1, and clause 4: the other user opens the first's
#   WinSta0; why the logon SID had to become each person's first),
#   SG_MUTANT_NO_LOGON_ACE (server/winstation.c).
#   WINE=... WINESERVER=... test/logondesk-users-gate.sh
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
W=$(mktemp -d /var/tmp/logondesk.XXXXXX)
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
    sg_gate_cgroup_kill
}
trap cleanup EXIT INT TERM

"$MINGW" -O1 -o "$W/logondesk-probe.exe" "$HERE/logondesk-probe.c" -luser32 -ladvapi32 -lntdll || { echo "FAIL build"; exit 1; }
chmod 755 "$W/logondesk-probe.exe"
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
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/logondesk-probe.exe" "$PFX/drive_c/logondesk-probe.exe"
# a user's first program sets the user up (profile, desktop): done first,
# for both, so the timing below is that of programs, not of first logons
for u in "$SG_USER1" "$SG_USER2"; do
    for i in 1 2 3; do
        as "$u" timeout -s KILL 300 "$WINE" 'C:\logondesk-probe.exe' info >"$W/warm-$u.out" 2>/dev/null && break
    done
done
probe() { local u=$1; shift; as "$u" timeout -s KILL 150 "$WINE" 'C:\logondesk-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
child() { v "$1" CHILD; }

as "$SG_USER1" timeout -s KILL 700 "$WINE" 'C:\logondesk-probe.exe' hold > "$W/hold.out" 2>/dev/null &
holdpid=$!
for i in $(seq 240); do grep -q HELD "$W/hold.out" 2>/dev/null && break; sleep 1; done
held=$(tr -d '\r' < "$W/hold.out")
s1=$(v "$held" SESSION)
[ "$(v "$held" HELD)" = 1 ] && [ -n "$s1" ] && [ "$s1" != 1 ] && pass "$SG_USER1 has a window in session $s1" \
    || fail "$SG_USER1 hold: $held"

o1=$(probe "$SG_USER1" info); o2=$(probe "$SG_USER2" info)
printf '      %s\n' $(printf '%s\n' "$o1" | tr ' ' '=')
u1=$(v "$o1" USER); l1=$(v "$o1" LOGON); l2=$(v "$o2" LOGON)
case "$l1" in S-1-5-5-0-*|'') fail "$SG_USER1's logon SID: '$l1'" ;; S-1-5-5-*) pass "$SG_USER1 has a logon SID of its own ($l1)" ;; *) fail "$SG_USER1's logon SID: '$l1'" ;; esac
case "$l2" in S-1-5-5-0-*|'') fail "$SG_USER2's logon SID: '$l2'" ;; S-1-5-5-*) [ "$l2" != "$l1" ] && pass "$SG_USER2's is another ($l2)" || fail "both users have $l1" ;; *) fail "$SG_USER2's logon SID: '$l2'" ;; esac
l1b=$(v "$(probe "$SG_USER1" info)" LOGON)
[ -n "$l1" ] && [ "$l1b" = "$l1" ] && pass "$SG_USER1's next process has the same logon SID" || fail "$SG_USER1's next process: '$l1b' (was '$l1')"
for o in WINSTA DESKTOP; do
    case $o in WINSTA) m=0xf037f ;; DESKTOP) m=0xf01ff ;; esac
    aces=$(printf '%s\n' "$o1" | sed -n "s/^$o ACE 0 //p" | awk '{print $1}' | sort | tr '\n' ' ')
    [ "$aces" = "$(printf '%s\n' S-1-5-18 S-1-5-32-544 "$u1" "$l1" | sort | tr '\n' ' ')" ] \
        && printf '%s\n' "$o1" | grep -q "^$o ACE 0 $l1 $m\$" \
        && pass "$o: SYSTEM, Administrators, $SG_USER1 and $SG_USER1's logon SID (full access), nobody else" \
        || fail "$o ACEs: $(printf '%s\n' "$o1" | grep "^$o ACE" | tr '\n' ' ')"
done

c=$(child "$(probe "$SG_USER1" limited)")
[ "$c" = 63 ] && pass "$SG_USER1's USER_LIMITED child is on the desktop, makes windows, and may open its desktop itself" \
    || fail "$SG_USER1's USER_LIMITED child: '$c'"

out=$(probe "$SG_USER2" open "$s1")
[ "$(v "$out" 'OPEN READ')" = "0 5" ] && [ "$(v "$out" 'OPEN CLIPBOARD')" = "0 5" ] \
    && pass "$SG_USER2 cannot open $SG_USER1's WinSta0, to read it or for its clipboard" \
    || fail "$SG_USER2 opening $SG_USER1's WinSta0: $(printf '%s\n' "$out" | grep OPEN | tr '\n' ' ')"
[ "$(v "$out" 'LIMITED OPEN')" = 0 ] && pass "...nor can $SG_USER2's USER_LIMITED child" \
    || fail "$SG_USER2's USER_LIMITED child opening $SG_USER1's WinSta0: '$(v "$out" 'LIMITED OPEN')' (bit 0 read, 1 clipboard)"
out=$(probe "$SG_USER1" open "$s1")
[ "$(v "$out" 'OPEN READ')" = "1 0" ] && [ "$(v "$out" 'LIMITED OPEN')" = 3 ] \
    && pass "$SG_USER1's own process and its USER_LIMITED child can" \
    || fail "$SG_USER1 opening its WinSta0: $(printf '%s\n' "$out" | grep OPEN | tr '\n' ' ')"
out=$(probe "$SG_SYSTEM" open "$s1")
[ "$(v "$out" 'OPEN READ')" = "1 0" ] && [ "$(v "$out" 'OPEN CLIPBOARD')" = "1 0" ] \
    && pass "SYSTEM can" || fail "SYSTEM opening $SG_USER1's WinSta0: $(printf '%s\n' "$out" | grep OPEN | tr '\n' ' ')"

# the holding process and what it started (sudo -> timeout -> wine), by PID
kids() { for k in $(ps -o pid= --ppid "$1"); do kids "$k"; echo "$k"; done; }
sudo -n kill -9 $(kids $holdpid) $holdpid 2>/dev/null
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
