#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# An administrator's split token, as on Windows (patches/sg/7737). An unelevated member of the
# administrators' group (sg-admins; the server's SG_ADMIN_GROUP names another) had a standard user's
# token -- elevation type Default, no Administrators group, no linked token -- so Mozilla's NSIS
# installers (UAC plugin) never asked for elevation, then failed their Program Files writes silently.
# As SYSTEM (SG_SYSTEM) owns the prefix and SG_USER1 uses it:
#  1. a standard user: elevation Default, no Administrators, no linked token
#  2. an unelevated administrator: Limited, not elevated, Administrators present only for deny
#     (CheckTokenMembership and IsUserAnAdmin say no), linked token Full and elevated, which may be
#     read but not used (no impersonation, no process token)
#  3. the same administrator elevated (as sg-elevated-run starts it): Full, elevated, Administrators
#     enabled, linked token Limited
# Exit 77 without the accounts or passwordless sudo.
# Mutants (server/token.c): SG_MUTANT_ADMIN_NOT_SPLIT, SG_MUTANT_ADMINS_NOT_DENY_ONLY,
#          SG_MUTANT_NO_LINKED_TOKEN; (dlls/shell32/shellord.c) SG_MUTANT_ADMIN_BY_PRESENCE.
#   WINE=... WINESERVER=... test/splittoken-gate.sh
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
W=$(mktemp -d /var/tmp/splittoken.XXXXXX)
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

"$MINGW" -O1 -o "$W/splittoken-probe.exe" "$HERE/splittoken-probe.c" -ladvapi32 -lshell32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/splittoken-probe.exe"
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
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/splittoken-probe.exe" "$PFX/drive_c/splittoken-probe.exe"
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }
uid1=$(id -u "$SG_USER1")
server() {   # ADMIN_GROUP: restart the server with this administrators' group
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
    as "$SG_SYSTEM" sh -c "umask 002; SG_ADMIN_GROUP='$1' '$WINESERVER' -p"
}
probe_user() { as "$SG_USER1" timeout -s KILL 120 "$WINE" 'C:\splittoken-probe.exe' 2>/dev/null | tr -d '\r'; }
probe_elevated() { as "$SG_SYSTEM" env SG_SESSION_UID="$uid1" SG_ELEVATED_IDENTITY=1 USER="$SG_USER1" timeout -s KILL 90 "$WINE" 'C:\splittoken-probe.exe' 2>/dev/null | tr -d '\r'; }

# 1. a standard user (the administrators' group names no such group)
server sg-no-such-group
out=$(probe_user)
echo "$out" | sed 's/^/      standard: /'
[ "$(v "$out" TYPE)" = 1 ] && [ "$(v "$out" ELEVATED)" = 0 ] && pass "standard user: elevation Default, not elevated" || fail "standard user: type $(v "$out" TYPE) elevated $(v "$out" ELEVATED)"
[ "$(v "$out" ADMINS)" = "absent 0" ] && [ "$(v "$out" MEMBER)" = 0 ] && pass "standard user: no Administrators" || fail "standard user: Administrators $(v "$out" ADMINS) member $(v "$out" MEMBER)"
v "$out" LINKED | grep -q '^none' && pass "standard user: no linked token" || fail "standard user: linked $(v "$out" LINKED)"

# 2. the same account as an administrator, unelevated
server "$SG_GROUP"
out=$(probe_user)
echo "$out" | sed 's/^/      admin: /'
[ "$(v "$out" TYPE)" = 3 ] && [ "$(v "$out" ELEVATED)" = 0 ] && pass "unelevated administrator: elevation Limited, not elevated" || fail "unelevated administrator: type $(v "$out" TYPE) elevated $(v "$out" ELEVATED)"
[ "$(v "$out" ADMINS)" = "present 0x10" ] && pass "unelevated administrator: Administrators for deny only" || fail "unelevated administrator: Administrators $(v "$out" ADMINS)"
[ "$(v "$out" MEMBER)" = 0 ] && [ "$(v "$out" ISADMIN)" = 0 ] && pass "unelevated administrator: CheckTokenMembership and IsUserAnAdmin say no" || fail "unelevated administrator: member $(v "$out" MEMBER) IsUserAnAdmin $(v "$out" ISADMIN)"
case "$(v "$out" LINKED)" in "2 1 level 1 admins 1") pass "unelevated administrator: linked token Full, elevated, identification level" ;; *) fail "unelevated administrator: linked $(v "$out" LINKED)" ;; esac
[ "$(v "$out" LINKED_IMPERSONATE)" = 0 ] && [ "$(v "$out" LINKED_DUPLICATE)" = 0 ] && pass "unelevated administrator: the linked token cannot be used (no impersonation, no process token)" || fail "the linked token is usable: impersonate $(v "$out" LINKED_IMPERSONATE) duplicate $(v "$out" LINKED_DUPLICATE)"

# 3. the administrator elevated
out=$(probe_elevated)
echo "$out" | sed 's/^/      elevated: /'
[ "$(v "$out" TYPE)" = 2 ] && [ "$(v "$out" ELEVATED)" = 1 ] && [ "$(v "$out" MEMBER)" = 1 ] && pass "elevated administrator: Full, elevated, Administrators" || fail "elevated administrator: type $(v "$out" TYPE) elevated $(v "$out" ELEVATED) member $(v "$out" MEMBER)"
case "$(v "$out" LINKED)" in "3 0 level 1 admins 0") pass "elevated administrator: linked token Limited" ;; *) fail "elevated administrator: linked $(v "$out" LINKED)" ;; esac

[ $fails = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $((fails > 0))
