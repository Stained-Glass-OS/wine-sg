#!/bin/bash
# Gate for wine-sg 0243: a standard user's time zone and locale lookups work in
# a shared (system) prefix.
#
# kernelbase opened HKLM's Time Zones and Control\Nls keys with KEY_ALL_ACCESS,
# which only administrators get there: for every signed-in user both handles
# were NULL, GetTimeZoneInformation returned TIME_ZONE_ID_INVALID with the
# zone's name an unresolved "@tzres.dll,-47168", GetTimeZoneInformationForYear
# failed (Firefox panicked "No such local time" on its first page), and the
# code page, locale and language group enumerations found nothing.
#
# This user owns the prefix (SYSTEM there); SG_OTHER (default sgconf, in
# SG_GROUP) is the standard user. Needs passwordless `sudo -u $SG_OTHER`.
#   WINE=... test/userlocale-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/userlocale.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/userlocale-probe.c" || { echo "FAIL build"; exit 1; }
chmod 755 "$W/probe.exe"
mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null

# the owner (SYSTEM) first: the reference
"$WINE" "$W/probe.exe" 2>/dev/null | tr -d '\r' > "$W/owner.out"
sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
    "$WINE" "$W/probe.exe" 2>/dev/null | tr -d '\r' > "$W/other.out"
echo "owner: $(paste -sd' ' "$W/owner.out")"
echo "user:  $(paste -sd' ' "$W/other.out")"
v() { sed -n "s/^$1 //p" "$2"; }
[ "$(v TZID "$W/other.out")" = ok ] && pass "GetTimeZoneInformation answers the standard user" \
    || fail "GetTimeZoneInformation for the standard user: $(v TZID "$W/other.out")"
[ "$(v FORYEAR "$W/other.out")" = 1 ] && pass "GetTimeZoneInformationForYear succeeds (what Firefox's Rust time code calls)" \
    || fail "GetTimeZoneInformationForYear for the standard user: $(v FORYEAR "$W/other.out")"
std=$(v STDNAME "$W/other.out")
[ -n "$std" ] && [ "${std#@}" = "$std" ] && [ "$std" = "$(v STDNAME "$W/owner.out")" ] \
    && pass "the zone's name is resolved, as the owner sees it ($std)" || fail "zone name '$std' (owner: '$(v STDNAME "$W/owner.out")')"
for k in CODEPAGES LOCALES GROUPS; do
    o=$(v $k "$W/other.out"); w=$(v $k "$W/owner.out")
    [ "${o:-0}" -gt 0 ] && [ "$o" = "$w" ] && pass "$k enumerated for the standard user as for the owner ($o)" \
        || fail "$k: standard user ${o:-none}, owner ${w:-none}"
done
echo "userlocale-gate: $fails failure(s)"
exit $((fails > 0))
