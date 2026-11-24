#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# The server's per-user security tables have no limit, and fail closed (patches/sg/9151).
#
# The descriptors that keep each user's HKCU theirs (5950, 9150) were kept in a table of 32: the
# 33rd user's hive had none and was open to every account again. Users' sessions were kept in a
# table of 200: the 201st user got SYSTEM's session. HKEY_USERS itself and HKEY_USERS\.Default
# (userdef.reg, the template of every new profile) had no descriptor at all. As in a real
# installation the prefix belongs to SYSTEM (SG_SYSTEM, default sgsystem); before SG_USER1
# (sgtest1) and SG_USER2 (sgtest2) ever connect, SYSTEM logs 40 other users' hives on (the keys
# HKEY_USERS\S-1-5-21-0-0-0-9100..9139) and starts programs in 200 other users' sessions
# (SG_SESSION_UID, as sg-elevated-run does), so the two come after both old limits:
#  1. SG_USER2 may not write SG_USER1's HKCU, nor another of the 40 hives; SG_USER1 writes it,
#     and its restricted (sandboxed) program reads it
#  2. SG_USER2 may read HKEY_USERS and HKEY_USERS\.Default, not create a hive in HKEY_USERS nor
#     write .Default, nor write a hive not named by a SID; SYSTEM may
#  3. every session SYSTEM made is a different one; SG_USER1's and SG_USER2's are their own,
#     not SYSTEM's and not each other's
# Exit 77 without the accounts or passwordless sudo.
# Mutants: SG_MUTANT_HIVE_SD_TABLE_32, SG_MUTANT_HKU_NOT_A_SID_OPEN (server/registry.c),
#   SG_MUTANT_SESSION_MAP_200 (server/process.c).
#   WINE=... WINESERVER=... test/manyusers-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
SG_SYSTEM=${SG_SYSTEM:-sgsystem}
SG_USER1=${SG_USER1:-sgtest1}
SG_USER2=${SG_USER2:-sgtest2}
SG_GROUP=${SG_GROUP:-sgwine}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
HIVES=${HIVES:-40}
SESSIONS=${SESSIONS:-200}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/sg-manyusers.XXXXXX)
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
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O1 -o "$W/manyusers-probe.exe" "$HERE/manyusers-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/manyusers-probe.exe"
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/manyusers-probe.exe" "$PFX/drive_c/manyusers-probe.exe"
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
P='C:\manyusers-probe.exe'
user1() { as "$SG_USER1" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }
user2() { as "$SG_USER2" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }
system() { as "$SG_SYSTEM" timeout -s KILL 90 "$WINE" "$P" "$@" 2>/dev/null | tr -d '\r'; }
export -f as
export SG_SYSTEM WINE WINESERVER PFX W P

# other users first: HIVES hives, SESSIONS sessions
out=$(system fill 9100 "$HIVES")
[ "$(v "$out" FILL)" = "$HIVES $HIVES" ] && pass "SYSTEM made and opened $HIVES other users' hives" || fail "fill: $out"
seq 70001 $((70000 + SESSIONS)) | xargs -P 6 -I{} bash -c \
    'as "$SG_SYSTEM" env SG_SESSION_UID={} timeout -s KILL 90 "$WINE" "$P" sess 2>/dev/null | tr -d "\r"' > "$W/sessions.txt"
n=$(grep -c '^SESSION ' "$W/sessions.txt"); u=$(grep '^SESSION ' "$W/sessions.txt" | sort -u | wc -l)
[ "$n" = "$SESSIONS" ] && [ "$u" = "$SESSIONS" ] && ! grep -qx 'SESSION 1' "$W/sessions.txt" \
    && pass "SYSTEM started programs in $SESSIONS other users' sessions, each its own" \
    || fail "$SESSIONS sessions: $n started, $u different$(grep -qx 'SESSION 1' "$W/sessions.txt" && echo ', some in SYSTEM'\''s')"

# now the two
for u in "$SG_USER1" "$SG_USER2"; do
    as "$u" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
done
out=$(user1 rread)
sid1=$(v "$out" USER)
case "$sid1" in S-1-5-21-*) ;; *) echo "FAIL  $SG_USER1 has no SID"; echo "RESULT: FAIL"; exit 1 ;; esac
echo "      $SG_USER1 is $sid1"

# 1. hives
H="HKU\\$sid1\\Software"
[ "$(v "$(user2 open "$H" 20006)" OPEN)" = 5 ] && [ "$(v "$(user2 mk "$H\\SgOther")" MK)" = 5 ] \
    && pass "$SG_USER2 may not write $SG_USER1's HKCU (hive $((HIVES + 2)) or later)" || fail "$SG_USER2 writes $SG_USER1's HKCU"
[ "$(v "$(user2 open "HKU\\S-1-5-21-0-0-0-$((9100 + HIVES - 1))\\Software" 20006)" OPEN)" = 5 ] \
    && pass "...nor the ${HIVES}th other user's" || fail "$SG_USER2 writes the ${HIVES}th other user's hive"
[ "$(v "$(user1 setval "HKCU\\Software")" SET)" = 0 ] && [ "$(v "$(user1 mk "HKCU\\Software\\SgMine")" MK)" = 0 ] \
    && [ "$(v "$(user1 del "HKCU\\Software\\SgMine")" DEL)" = 0 ] \
    && pass "$SG_USER1 writes, creates and deletes in their own HKCU" || fail "$SG_USER1 is locked out of their HKCU"
[ "$(v "$out" RREAD)" = 0 ] && pass "...and their restricted (sandboxed) program reads it" || fail "restricted read: $out"

# 2. HKEY_USERS itself, .Default, hives not named by a SID
[ "$(v "$(user2 open HKU 20019)" OPEN)" = 0 ] && [ "$(v "$(user2 open 'HKU\.Default\Software' 20019)" OPEN)" = 0 ] \
    && pass "$SG_USER2 reads HKEY_USERS and HKEY_USERS\\.Default" || fail "$SG_USER2 cannot read HKEY_USERS or .Default"
mk=$(v "$(user2 mk 'HKU\S-1-5-21-0-0-0-9999')" MK)
[ "$mk" = 5 ] && pass "$SG_USER2 may not create a hive in HKEY_USERS" || fail "$SG_USER2 creates keys in HKEY_USERS ($mk)"
[ "$(v "$(user2 setval 'HKU\.Default\Software')" SET)" = 5 ] && [ "$(v "$(user2 mk 'HKU\.Default\Software\SgOther')" MK)" = 5 ] \
    && pass "...nor write HKEY_USERS\\.Default (every new profile's template)" || fail "$SG_USER2 writes HKEY_USERS\\.Default"
long='S-1-5-21-0-0-0-9100-1-2-3-4-5-6-7-8-9-10-11'
[ "$(v "$(system mk "HKU\\$long\\Software")" MK)" = 0 ] && [ "$(v "$(system mk 'HKU\S-1-5-21-0-0-0-9100x\Software')" MK)" = 0 ] \
    && [ "$(v "$(system setval 'HKU\.Default\Software')" SET)" = 0 ] \
    && pass "SYSTEM creates hives in HKEY_USERS and writes .Default" || fail "SYSTEM cannot write HKEY_USERS"
[ "$(v "$(user2 open "HKU\\$long\\Software" 20006)" OPEN)" = 5 ] && [ "$(v "$(user2 open 'HKU\S-1-5-21-0-0-0-9100x\Software' 20006)" OPEN)" = 5 ] \
    && pass "$SG_USER2 may not write a hive whose name is not a SID" || fail "$SG_USER2 writes a hive not named by a SID"

# 3. sessions
s0=$(v "$(system sess)" SESSION); s1=$(v "$(user1 sess)" SESSION); s2=$(v "$(user2 sess)" SESSION)
echo "      sessions: SYSTEM $s0, $SG_USER1 $s1, $SG_USER2 $s2"
[ -n "$s1" ] && [ -n "$s2" ] && [ "$s1" != "$s0" ] && [ "$s2" != "$s0" ] && [ "$s1" != "$s2" ] \
    && ! grep -qx "SESSION $s1" "$W/sessions.txt" && ! grep -qx "SESSION $s2" "$W/sessions.txt" \
    && pass "$SG_USER1 and $SG_USER2 (after $SESSIONS others) have sessions of their own, not SYSTEM's" \
    || fail "$SG_USER1/$SG_USER2 share a session (SYSTEM $s0, $s1, $s2)"

as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
