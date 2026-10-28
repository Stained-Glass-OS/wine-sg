#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# An elevated program is the person who asked for it, elevated (patches/sg/7601).
#
# On Windows, consent gives the same user a full token. Here an elevated
# program runs on disk as the SYSTEM account, and it had SYSTEM's token too:
# its HKCU, profile and Start menu were SYSTEM's, and what an installer
# started when done (MediaMonkey, Dropbox) ran as SYSTEM in the person's
# session. As in a real installation the prefix belongs to SYSTEM (SG_SYSTEM,
# default sgsystem) and a standard user (SG_USER1, default sgtest1) uses it;
# SYSTEM starts the probe in the user's session as sg-elevated-run does
# (SG_SESSION_UID, SG_ELEVATED_IDENTITY=1, USER=the person):
#  1. it has the person's SID, Administrators, elevation Full, high integrity,
#     the person's USERNAME and USERPROFILE
#  2. what it writes to HKCU the person reads from their own HKCU
#  3. the program it starts is the same person, elevated
#  4. without SG_ELEVATED_IDENTITY it is SYSTEM, as before
#  5. a standard user asking for it gets nothing
#  6. a SYSTEM folder gives Administrators full access, and the elevated
#     token may write there (AccessCheck)
# Exit 77 without the accounts or passwordless sudo.
# Mutants: SG_MUTANT_ELEVATED_AS_SYSTEM (server/process.c),
#          SG_MUTANT_FILE_NO_ADMINS (server/file.c).
#   WINE=... WINESERVER=... test/elevid-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}
SG_USER1=${SG_USER1:-sgtest1}
SG_GROUP=${SG_GROUP:-sgwine}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/elevid.XXXXXX)
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

"$MINGW" -O1 -o "$W/elevid-probe.exe" "$HERE/elevid-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/elevid-probe.exe"
for u in "$SG_SYSTEM" "$SG_USER1"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
as "$SG_USER1" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/elevid-probe.exe" "$PFX/drive_c/elevid-probe.exe"
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
uid1=$(id -u "$SG_USER1")
elevated() { as "$SG_SYSTEM" env SG_SESSION_UID="$uid1" SG_ELEVATED_IDENTITY=1 USER="$SG_USER1" timeout -s KILL 90 "$WINE" "$@" 2>/dev/null | tr -d '\r'; }

user=$(as "$SG_USER1" timeout -s KILL 120 "$WINE" 'C:\elevid-probe.exe' id 2>/dev/null | tr -d '\r')
sid1=$(v "$user" SID)
echo "      $SG_USER1: $(printf '%s' "$user" | tr '\n' ' ')"
case "$sid1" in S-1-5-21-*) ;; *) echo "FAIL  $SG_USER1 has no SID: $user"; echo "RESULT: FAIL"; exit 1 ;; esac

out=$(elevated 'C:\elevid-probe.exe' id)
echo "      elevated: $(printf '%s' "$out" | tr '\n' ' ')"
[ "$(v "$out" SID)" = "$sid1" ] && pass "an elevated program is $SG_USER1 ($sid1), not SYSTEM" || fail "elevated SID $(v "$out" SID), not $sid1"
[ "$(v "$out" ADMIN)" = 1 ] && [ "$(v "$out" ELEVATION)" = 2 ] && [ "$(v "$out" INTEGRITY)" = 3000 ] \
    && pass "...elevated: Administrators, elevation type Full, high integrity" || fail "elevated token: $out"
[ "$(v "$out" USERNAME)" = "$SG_USER1" ] && case "$(v "$out" USERPROFILE)" in *"\\$SG_USER1") true ;; *) false ;; esac \
    && pass "...with $SG_USER1's USERNAME and USERPROFILE ($(v "$out" USERPROFILE))" || fail "elevated profile: $(v "$out" USERNAME) $(v "$out" USERPROFILE)"
[ "$(v "$out" SESSION)" = "$(v "$user" SESSION)" ] && pass "...in $SG_USER1's session" || fail "elevated session $(v "$out" SESSION), not $(v "$user" SESSION)"

out=$(elevated 'C:\elevid-probe.exe' hkcu-write elevated-7601)
back=$(as "$SG_USER1" timeout -s KILL 90 "$WINE" 'C:\elevid-probe.exe' hkcu-read 2>/dev/null | tr -d '\r')
[ "$(v "$out" WROTE)" = 1 ] && [ "$(v "$back" BY)" = elevated-7601 ] \
    && pass "what it writes to HKCU is in $SG_USER1's own HKCU" || fail "HKCU: wrote '$out', $SG_USER1 reads '$back'"

out=$(elevated cmd /c 'C:\elevid-probe.exe' id)
[ "$(v "$out" SID)" = "$sid1" ] && [ "$(v "$out" ADMIN)" = 1 ] \
    && pass "the program it starts is $SG_USER1, elevated, too" || fail "child: $(printf '%s' "$out" | tr '\n' ' ')"

out=$(as "$SG_SYSTEM" env SG_SESSION_UID="$uid1" timeout -s KILL 90 "$WINE" 'C:\elevid-probe.exe' id 2>/dev/null | tr -d '\r')
[ "$(v "$out" SID)" = S-1-5-18 ] && pass "without SG_ELEVATED_IDENTITY: SYSTEM, as before" || fail "unasked: $(v "$out" SID)"

out=$(as "$SG_USER1" env SG_SESSION_UID="$uid1" SG_ELEVATED_IDENTITY=1 timeout -s KILL 90 "$WINE" 'C:\elevid-probe.exe' id 2>/dev/null | tr -d '\r')
[ "$(v "$out" SID)" = "$sid1" ] && [ "$(v "$out" ADMIN)" = 0 ] \
    && pass "a standard user asking for it stays a standard user" || fail "standard user: $(printf '%s' "$out" | tr '\n' ' ')"

out=$(elevated 'C:\elevid-probe.exe' files 'C:\Program Files')
[ "$(v "$out" ADMINS_FULL)" = 1 ] && pass "C:\\Program Files gives Administrators full access, as on Windows" || fail "Program Files ACL: $out"
[ "$(v "$out" MAY_WRITE)" = 1 ] && pass "...and the elevated program may write there (AccessCheck)" || fail "AccessCheck: $out"

as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
