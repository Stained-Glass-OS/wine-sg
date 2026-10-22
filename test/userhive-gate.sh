#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A user's own HKCU after an elevated program wrote there (patches/sg/5948), and a 32-bit program's
# HKCU\Software (5949), in a shared prefix.
# An elevated program runs as SYSTEM with the user's HKCU. The server let SYSTEM stamp the machine
# descriptor (users read) onto an existing key, and gave the keys it created SYSTEM's: the user then
# could not open HKCU\Software, nor create anything below it, until the server restarted. Every Burn
# bundle failed ("Failed to open uninstall registry key", 0x80070005: Python, .NET Desktop Runtime).
# And once a program had created HKCU\Software\WOW6432Node, Wine sent every 32-bit program there:
# Windows shares HKCU\Software between the views (since Windows 7).
# With the user $SG_USER1 and SYSTEM ($SG_SYSTEM, the prefix owner):
#   SYSTEM writes keys in the user's hive (HKU\<SID>\Software\..., existing and new ones)
#   the user: opens HKCU\Software, writes below SYSTEM's keys, creates keys and Uninstall entries
#   a planted HKCU\Software\WOW6432Node: a 32-bit program's HKCU\Software\... is still the shared key,
#     and what it writes the 64-bit view sees, also after the server restarts; HKLM\Software\Microsoft
#     in the 32-bit view is still below Wow6432Node
#   WINE=... WINESERVER=... test/userhive-gate.sh
# Mutants (server): SG_MUTANT_USERHIVE_STAMP, SG_MUTANT_HKCU_WOW_REDIRECT.
# Needs passwordless sudo and the accounts of usersessions-gate.sh; exit 77 otherwise.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_GROUP=${SG_GROUP:-sgwine}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs both mingw compilers"; exit 77; }
W=$(mktemp -d /var/tmp/userhive.XXXXXX)
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

x86_64-w64-mingw32-gcc -O1 -o "$W/uh64.exe" "$HERE/userhive-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
i686-w64-mingw32-gcc -O1 -o "$W/uh32.exe" "$HERE/userhive-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
for u in "$SG_SYSTEM" "$SG_USER1"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
for e in uh64 uh32; do sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/$e.exe" "$PFX/drive_c/$e.exe"; done
run() { local u=$1; shift; as "$u" timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | K="$2" awk 'BEGIN { k = ENVIRON["K"] } index($0, k " ") == 1 { print substr($0, length(k) + 2); exit }'; }
restart() {
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
    as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
}

# the user's hive, loaded as at sign-in, with the keys a session has
run "$SG_USER1" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall' /f >/dev/null
SID=$(v "$(run "$SG_USER1" 'C:\uh64.exe' sid)" SID)
[ -n "$SID" ] || { fail "no SID for $SG_USER1"; exit 1; }
# the server restarted (as at boot): the hive's keys come from its file, without descriptors
restart
run "$SG_USER1" reg query 'HKCU\Software' >/dev/null
# an elevated program (SYSTEM) writes in it: keys that exist (Software, Uninstall) and new ones
for k in "HKU\\$SID\\Software" "HKU\\$SID\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall" \
         "HKU\\$SID\\Software\\SgElevated\\Sub" "HKU\\$SID\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgElevApp"; do
    run "$SG_SYSTEM" reg add "$k" /v SgBy /d system /f >/dev/null
done
out=$(run "$SG_SYSTEM" reg query "HKU\\$SID\\Software\\SgElevated\\Sub" /v SgBy)
case "$out" in *system*) pass "SYSTEM wrote keys in $SG_USER1's hive (HKU\\$SID)" ;; *) fail "SYSTEM could not write the user's hive: $out" ;; esac

out=$(run "$SG_USER1" 'C:\uh64.exe' access)
[ "$(v "$out" "OPENREAD Software ->")" = 0 ] && pass "$SG_USER1 still opens HKCU\\Software" || fail "HKCU\\Software: $(v "$out" "OPENREAD Software ->") (want 0)"
[ "$(v "$out" "CREATE Software\\SgUserNew ->")" = 0 ] && pass "...creates a key below it" || fail "create under HKCU\\Software: $(v "$out" "CREATE Software\\SgUserNew ->")"
[ "$(v "$out" "OPENRW Software\\SgElevated\\Sub ->")" = 0 ] && [ "$(v "$out" "CREATE Software\\SgElevated\\Sub\\Mine ->")" = 0 ] \
    && pass "...writes the keys SYSTEM made there" || fail "SYSTEM's new keys: $out"
[ "$(v "$out" "OPENRW Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall ->")" = 0 ] && \
[ "$(v "$out" "CREATE Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgUserApp ->")" = 0 ] \
    && pass "...and its Uninstall key, which SYSTEM wrote too (Burn's detection)" || fail "Uninstall: $out"

# a program creates HKCU\Software\WOW6432Node\... (as a Windows program may: an ordinary key there)
out=$(run "$SG_USER1" 'C:\uh64.exe' plant)
[ "$(v "$out" "PLANT Software\\WOW6432Node\\SgPlanted ->")" = 0 ] || fail "plant: $out"
for when in "with it planted" "after the server restarted"; do
    out=$(run "$SG_USER1" 'C:\uh32.exe' view)
    [ "$(v "$out" "VIEW32 Software\\Microsoft\\Windows\\CurrentVersion ->")" = shared ] && [ "$(v "$out" "VIEW32 Software\\SgUserNew ->")" = shared ] \
        && pass "$when: a 32-bit program's HKCU\\Software is the shared key" || fail "$when: 32-bit HKCU view: $out"
    [ "$(v "$out" "CREATE32 Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgUser32 ->")" = 0 ] && \
    [ "$(v "$out" "OPEN64 Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SgUser32 ->")" = 0 ] \
        && pass "...what it writes there the 64-bit view sees" || fail "$when: 32-bit write: $out"
    [ "$(v "$out" "VIEW32 Software\\Microsoft ->")" = Wow6432Node ] && pass "...and HKLM\\Software is still redirected" || fail "$when: HKLM 32-bit view: $out"
    run "$SG_USER1" reg delete 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\SgUser32' /f /reg:64 >/dev/null
    restart
done

[ $fails = 0 ] && { echo "userhive gate: PASS"; exit 0; }
echo "userhive gate: $fails FAIL"; exit 1
