#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A package installed "just for me" by a standard user of a shared prefix (patches/sg/5944, 5945).
# Python 3.13 from the SG Store failed with ERROR_ACCESS_DENIED: Wine's msi runs as the user, and the
# machine keys (3764) refused it the per-user registration Windows' Installer service writes as
# SYSTEM -- the components under HKLM\...\Installer\UserData\<SID>, and the Apps & features entry
# under HKLM\...\Uninstall. The user's own UserData subtree is now theirs (also its keys reloaded from
# system.reg without a descriptor), and a per-user product's entry the user may not put in HKLM goes
# to HKCU. With the standard users $SG_USER1, $SG_USER2 and SYSTEM ($SG_SYSTEM, the prefix owner):
#   $SG_USER2 cannot make $SG_USER1's UserData key ahead of it; $SG_USER1 installs a per-user package: installed, its entry in HKCU; the prefix restarted as at
#   boot; still installed, uninstalls (entry gone), installs a second; $SG_USER2 cannot write
#   $SG_USER1's UserData.
#   WINE=... WINESERVER=... test/msiperuser-gate.sh
# Mutants: server SG_MUTANT_USERDATA_READONLY, msi SG_MUTANT_MSI_NO_USER_ARP.
# Needs passwordless sudo, wixl and the accounts of usersessions-gate.sh; exit 77 otherwise.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_USER2=${SG_USER2:-sgtest2}; SG_GROUP=${SG_GROUP:-sgwine}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v wixl >/dev/null || { echo "SKIP: wixl missing (apt install wixl)"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
W=$(mktemp -d /var/tmp/msiperuser.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" XDG_RUNTIME_DIR="$W/run-$u" "$@"
}
cleanup() {
    set +e
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

G1=7A1E5C3B-1F2D-4E6A-9B8C-0D1E2F3A4B51 G2=7A1E5C3B-1F2D-4E6A-9B8C-0D1E2F3A4B61
C1="{$G1}" C2="{$G2}"
(cd "$W" && echo hello > hello.txt &&
 wixl -D Code="$G1" -D N=1 -D Upgrade=7A1E5C3B-1F2D-4E6A-9B8C-0D1E2F3A4B52 -D Comp=7A1E5C3B-1F2D-4E6A-9B8C-0D1E2F3A4B53 \
     -o pu1.msi "$HERE/msiperuser.wxs" 2>/dev/null &&
 wixl -D Code="$G2" -D N=2 -D Upgrade=7A1E5C3B-1F2D-4E6A-9B8C-0D1E2F3A4B62 -D Comp=7A1E5C3B-1F2D-4E6A-9B8C-0D1E2F3A4B63 \
     -o pu2.msi "$HERE/msiperuser.wxs" 2>/dev/null) || { echo "FAIL  the test packages did not build"; exit 1; }
x86_64-w64-mingw32-gcc -O1 -municode -o "$W/probe.exe" "$HERE/msiperuser-probe.c" -lmsi -ladvapi32 || { echo "FAIL build"; exit 1; }

for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
restart() {   # the machine server restarted as at boot: keys come back from system.reg without descriptors
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
    as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
    as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
    sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
}
restart
# SYSTEM touches UserData (its own per-machine installs do): now with a descriptor, until the next start
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 120 '$WINE' reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\Installer\UserData' /f" >/dev/null 2>&1
for u in "$SG_USER1" "$SG_USER2"; do
    as "$u" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
done
for f in pu1.msi pu2.msi probe.exe; do sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 644 "$W/$f" "$PFX/drive_c/$f"; done
run() { local u=$1; shift; as "$u" timeout -s KILL 300 "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | K="$2" awk 'BEGIN { k = ENVIRON["K"] } index($0, k " ") == 1 { print substr($0, length(k) + 2); exit }'; }
msi() { as "$1" timeout -s KILL 300 "$WINE" msiexec "$2" "$3" /qn >/dev/null 2>&1; echo $?; }

# --- before $SG_USER1 has any: $SG_USER2 cannot make $SG_USER1's key in UserData (and so own it)
SID1=$(v "$(run "$SG_USER1" 'C:\probe.exe' "$C1")" SID)
out=$(run "$SG_USER2" 'C:\probe.exe' "$C1" "$SID1")
{ [ -n "$SID1" ] && [ "$(v "$out" OTHER)" = 5 ]; } && pass "$SG_USER2 cannot make $SG_USER1's UserData key ahead of it" \
    || fail "$SG_USER2 made $SG_USER1's UserData key: $(printf '%s' "$out" | tr '\n' ' ')"

# --- the first per-user install
rc=$(msi "$SG_USER1" /i 'C:\pu1.msi')
out=$(run "$SG_USER1" 'C:\probe.exe' "$C1")
{ [ "$rc" = 0 ] && [ "$(v "$out" STATE)" = 5 ]; } && pass "$SG_USER1 installs a per-user package (msiexec $rc, state 5)" \
    || fail "per-user install: msiexec $rc, $(printf '%s' "$out" | tr '\n' ' ')"
{ [ "$(v "$out" ARP_HKCU)" = 0 ] && [ "$(v "$out" ARP_HKLM)" != 0 ]; } && pass "...its Apps & features entry is the user's (HKCU), not in HKLM" \
    || fail "entry: HKLM $(v "$out" ARP_HKLM) HKCU $(v "$out" ARP_HKCU)"
sudo -n test -f "$PFX/drive_c/users/$SG_USER1/AppData/Local/SGPerUser1/hello.txt" \
    && pass "...its file is in the user's AppData\\Local" || fail "no file in $SG_USER1's AppData\\Local"

# --- after a restart: its registration reloaded without descriptors
restart
out=$(run "$SG_USER1" 'C:\probe.exe' "$C1")
[ "$(v "$out" STATE)" = 5 ] && pass "after a restart it is still installed" || fail "after restart: $(printf '%s' "$out" | tr '\n' ' ')"
rc=$(msi "$SG_USER1" /x "$C1")
out=$(run "$SG_USER1" 'C:\probe.exe' "$C1")
{ [ "$rc" = 0 ] && [ "$(v "$out" STATE)" = -1 ] && [ "$(v "$out" ARP_HKCU)" = 2 ]; } \
    && pass "...the user uninstalls it (msiexec $rc): unknown, its entry gone" \
    || fail "uninstall after restart: msiexec $rc, $(printf '%s' "$out" | tr '\n' ' ')"
rc=$(msi "$SG_USER1" /i 'C:\pu2.msi')
out=$(run "$SG_USER1" 'C:\probe.exe' "$C2")
{ [ "$rc" = 0 ] && [ "$(v "$out" STATE)" = 5 ]; } && pass "...and installs another per-user package (msiexec $rc)" \
    || fail "second install after restart: msiexec $rc, $(printf '%s' "$out" | tr '\n' ' ')"

# --- another user's registration is not $SG_USER2's to write
out=$(run "$SG_USER2" 'C:\probe.exe' "$C2" "$SID1")
{ [ -n "$SID1" ] && [ "$(v "$out" OTHER)" = 5 ] && [ "$(v "$out" SID)" != "$SID1" ]; } \
    && pass "$SG_USER2 cannot write $SG_USER1's UserData ($SID1)" || fail "$SG_USER2 on $SG_USER1's UserData: $(printf '%s' "$out" | tr '\n' ' ')"
out=$(run "$SG_USER1" 'C:\probe.exe' "$C2" "$SID1")
[ "$(v "$out" OTHER)" = 0 ] && pass "...$SG_USER1 can" || fail "$SG_USER1 on its own UserData: $(v "$out" OTHER)"

[ $fails = 0 ] && echo "msiperuser-gate: all passed" || echo "msiperuser-gate: $fails failure(s)"
exit $((fails != 0))
