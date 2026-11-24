#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# What an elevated program creates is Administrators', as on Windows (patches/sg/9150).
#
# An elevated program had the person's token with the person's default DACL and, as every token,
# this machine's Domain Users as the owner of what it creates: the keys an elevated installer wrote
# (HKLM\Software\Classes\.7z) the same person could change and delete unelevated, and since every
# account holds Domain Users, every account owned every key made at run time -- a standard user
# could rewrite the DACL of HKLM\Software\Classes. Keys of a user's hive loaded from its file were
# open to every account. An unelevated program could open its person's elevated program to write
# its memory. As in a real installation the prefix belongs to SYSTEM (SG_SYSTEM, default sgsystem),
# SG_USER1 (sgtest1) and SG_USER2 (sgtest2) use it; SYSTEM starts the probe in SG_USER1's session,
# elevated, as sg-elevated-run does (SG_SESSION_UID, SG_ELEVATED_IDENTITY=1):
#  1. the elevated token's owner is Administrators and its default DACL names SYSTEM and
#     Administrators, not the person; an unelevated token's owner is its user, not Domain Users
#  2. a machine key it creates is Administrators': SG_USER1 unelevated may not write it, change its
#     DACL or delete it; elevated it may
#  3. a key SYSTEM creates is not every account's: SG_USER1 may not change its DACL
#  4. a key it creates in the person's HKCU stays the person's, also after it sets its owner;
#     so does its key in the person's own Installer\UserData\<SID> (per-user packages)
#  5. another account may not write SG_USER1's HKCU; SG_USER1's sandboxed (restricted) programs
#     read it and do not write it
#  6. SG_USER1 unelevated may not open the elevated program for writing its memory
# Exit 77 without the accounts or passwordless sudo.
# Mutants (server/token.c): SG_MUTANT_ELEVATED_DACL_HAS_USER, SG_MUTANT_OWNER_PRIMARY_GROUP,
#   SG_MUTANT_ELEVATED_PROCESS_OPEN; (server/registry.c) SG_MUTANT_USERKEY_UNCHECKED,
#   SG_MUTANT_SETSD_TOKEN_DEFAULTS, SG_MUTANT_ELEVATED_USERDATA_ADMIN, SG_MUTANT_USERKEY_NO_SANDBOX_READ.
#   WINE=... WINESERVER=... test/elevobj-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
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
W=$(mktemp -d /var/tmp/elevobj.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" "$@"
}
cleanup() {
    set +e
    [ -n "${bg:-}" ] && kill "$bg" 2>/dev/null
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O1 -o "$W/elevobj-probe.exe" "$HERE/elevobj-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/elevobj-probe.exe"
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
for u in "$SG_USER1" "$SG_USER2"; do
    as "$u" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
done
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/elevobj-probe.exe" "$PFX/drive_c/elevobj-probe.exe"
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
w() { v "$@" | cut -d' ' -f1; }
uid1=$(id -u "$SG_USER1")
P='C:\elevobj-probe.exe'
elevated() { as "$SG_SYSTEM" env SG_SESSION_UID="$uid1" SG_ELEVATED_IDENTITY=1 USER="$SG_USER1" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }
user1() { as "$SG_USER1" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }
user2() { as "$SG_USER2" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }
system() { as "$SG_SYSTEM" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }

sid1=$(v "$(user1 tok)" USER)
case "$sid1" in S-1-5-21-*) ;; *) echo "FAIL  $SG_USER1 has no SID"; echo "RESULT: FAIL"; exit 1 ;; esac
echo "      $SG_USER1 is $sid1"

# 1. tokens
out=$(elevated tok)
own=$(v "$out" OWNER); dacl=$(v "$out" DEFDACL)
echo "      elevated: owner $own, default DACL $dacl"
[ "$own" = S-1-5-32-544 ] && pass "an elevated program's objects are owned by Administrators" || fail "elevated owner $own"
case "$dacl" in *"$sid1"*|"") fail "elevated default DACL names $SG_USER1 (or none): $dacl" ;;
    *"(A;;GA;;;SY)"*"(A;;GA;;;BA)"*) pass "...and its default DACL gives SYSTEM and Administrators, not $SG_USER1" ;;
    *) fail "elevated default DACL: $dacl" ;; esac
out=$(user1 tok)
[ "$(v "$out" OWNER)" = "$sid1" ] && pass "an unelevated program's objects are owned by its user, not Domain Users" \
    || fail "unelevated owner $(v "$out" OWNER)"

# 2. a machine key an elevated program creates
K='HKLM\Software\SgElevObj'
[ "$(v "$(elevated mk "$K\\Sub")" MK)" = 0 ] || fail "elevated could not create $K\\Sub"
out=$(elevated sd "$K\\Sub")
case "$(v "$out" SD)" in O:BA*) case "$(v "$out" SD)" in *"$sid1"*) fail "the elevated key grants $SG_USER1: $out" ;;
    *) pass "the key it creates is Administrators' and does not name $SG_USER1" ;; esac ;;
    *) fail "the elevated key: $out" ;; esac
[ "$(v "$(user1 open "$K\\Sub" 20019)" OPEN)" = 0 ] && pass "$SG_USER1 may read it" || fail "$SG_USER1 cannot read it"
[ "$(v "$(user1 setval "$K\\Sub")" SET)" = 5 ] && pass "$SG_USER1 unelevated may not write it" || fail "$SG_USER1 unelevated wrote it"
[ "$(v "$(user1 open "$K\\Sub" 40000)" OPEN)" = 5 ] && pass "...nor change its DACL" || fail "$SG_USER1 unelevated opened it WRITE_DAC"
[ "$(v "$(user1 del "$K\\Sub")" DEL)" = 5 ] && pass "...nor delete it" || fail "$SG_USER1 unelevated deleted it"
[ "$(v "$(user2 open "$K\\Sub" 10000)" OPEN)" = 5 ] && pass "...nor may $SG_USER2" || fail "$SG_USER2 opened it DELETE"
elevated mk "$K\\Sub" >/dev/null
[ "$(v "$(elevated del "$K\\Sub")" DEL)" = 0 ] && [ "$(v "$(elevated del "$K")" DEL)" = 0 ] \
    && pass "elevated, $SG_USER1 deletes it" || fail "elevated could not delete it"

# 3. a key SYSTEM creates
[ "$(v "$(system mk 'HKLM\Software\SgSysObj')" MK)" = 0 ] || fail "SYSTEM could not create a key"
[ "$(v "$(user1 open 'HKLM\Software\SgSysObj' 40000)" OPEN)" = 5 ] && [ "$(v "$(user1 open 'HKLM\Software\SgSysObj' 10000)" OPEN)" = 5 ] \
    && pass "a key SYSTEM creates: $SG_USER1 may neither change its DACL nor delete it" \
    || fail "a key SYSTEM creates is $SG_USER1's to change or delete: $(user1 sd 'HKLM\Software\SgSysObj')"

# 4. the person's own things
U='HKCU\Software\SgElevObjU'
elevated mk "$U\\Sub" >/dev/null
[ "$(v "$(elevated owner "$U\\Sub")" OWNERSET)" = 0 ] || fail "elevated could not set the owner of $U\\Sub"
[ "$(v "$(user1 setval "$U\\Sub")" SET)" = 0 ] && [ "$(v "$(user1 setval "$U")" SET)" = 0 ] \
    && pass "a key it creates in $SG_USER1's HKCU (also after setting its owner) stays $SG_USER1's" \
    || fail "$SG_USER1 cannot write the HKCU key the elevated program made: $(user1 sd "$U\\Sub")"
[ "$(v "$(user1 del "$U\\Sub")" DEL)" = 0 ] && [ "$(v "$(user1 del "$U")" DEL)" = 0 ] \
    && pass "...and $SG_USER1 deletes it" || fail "$SG_USER1 cannot delete the HKCU key"
D="HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\$sid1\\Products\\SgElevObj"
[ "$(v "$(elevated mk "$D")" MK)" = 0 ] || fail "elevated could not create $D"
[ "$(v "$(user1 setval "$D")" SET)" = 0 ] && [ "$(v "$(user1 del "$D")" DEL)" = 0 ] \
    && pass "its key in $SG_USER1's own Installer\\UserData stays $SG_USER1's (per-user packages)" \
    || fail "$SG_USER1 cannot use their UserData key: $(user1 sd "$D")"

# 5. a user's hive
H="HKU\\$sid1\\Software"
[ "$(v "$(user2 open "$H" 20006)" OPEN)" = 5 ] && [ "$(v "$(user2 mk "$H\\SgOther")" MK)" = 5 ] \
    && pass "$SG_USER2 may not write $SG_USER1's HKCU" || fail "$SG_USER2 writes $SG_USER1's HKCU"
out=$(user1 rread)
[ "$(v "$out" RREAD)" = 0 ] && [ "$(v "$out" RWRITE)" = 5 ] \
    && pass "$SG_USER1's restricted (sandboxed) program reads its HKCU and does not write it" \
    || fail "restricted token on HKCU: $(printf '%s' "$out" | tr '\n' ' ')"

# 6. the elevated program itself
as "$SG_SYSTEM" env SG_SESSION_UID="$uid1" SG_ELEVATED_IDENTITY=1 USER="$SG_USER1" timeout -s KILL 60 "$WINE" "$P" pid 30 \
    > "$W/pid.txt" 2>/dev/null &
bg=$!
for i in $(seq 60); do grep -q '^PID' "$W/pid.txt" 2>/dev/null && break; sleep 0.5; done
pid=$(sed -n 's/^PID \([0-9]*\).*/\1/p' "$W/pid.txt" | tr -d '\r')
if [ -n "$pid" ]; then
    out=$(user1 proc "$pid")
    [ "$(w "$out" PROC)" = 5 ] && pass "$SG_USER1 unelevated may not open the elevated program to write its memory" \
        || fail "$SG_USER1 unelevated opened the elevated program: $out"
    out=$(elevated proc "$pid")
    [ "$(w "$out" PROC)" = 0 ] && pass "...elevated, $SG_USER1 may" || fail "elevated could not open it: $out"
else fail "the elevated program did not start"; fi
kill "$bg" 2>/dev/null; bg=

as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
