#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# No account may read another person's registry hive file (patches/sg/9100).
# On Windows another user's NTUSER.DAT is closed to everyone but that person,
# SYSTEM and Administrators. In a shared prefix the machine's server writes
# every person's user-<uid>.reg; made with its umask (0644, systemd's 022) any
# account in the Wine group could read another's HKCU. The second Unix user
# SG_OTHER (default sgconf, in SG_GROUP) is the person; the server runs as the
# caller with umask 022, as sg-wineserver.service does:
#   1. SG_OTHER's hive, once written, is closed to group and others (0600) and
#      SG_OTHER cannot read the file -- yet reads its value through the server
#   2. a hive left 0644 by an older server is closed when the server loads it,
#      with nothing written to it, and its values are all there
#   3. the server owner's own hive is closed too; system.reg and userdef.reg
#      keep their modes (not people's hives)
#   4. a hive that is a symlink to a file of the server's is not chmod'ed
#      through the link
#   WINE=... WINESERVER=... test/hiveprivate-gate.sh
# Mutant: SG_MUTANT_HIVE_SHARED (server/registry.c). Needs passwordless
# sudo -u $SG_OTHER; exit 77 otherwise.
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no sudo -u $SG_OTHER"; exit 77; }
OUID=$(id -u "$SG_OTHER")
W=$(mktemp -d /var/tmp/sg-hiveprivate.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; sleep 1
    sudo -n -u "$SG_OTHER" rm -rf "$W/otherhome" 2>/dev/null
    find "$PFX" -maxdepth 1 -user "$SG_OTHER" -exec sudo -n -u "$SG_OTHER" rm -rf {} + 2>/dev/null
    chmod -R u+w "$W" 2>/dev/null; rm -rf "$W" 2>/dev/null
}
trap cleanup EXIT
mkdir "$PFX" "$W/otherhome"
chgrp "$SG_GROUP" "$PFX"; chmod 3770 "$PFX"; chmod 777 "$W/otherhome"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
start_server() { sg "$SG_GROUP" -c "umask 022; '$WINESERVER' -p" 2>>"$W/server.log"; }
stop_server() { WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; WINEPREFIX=$PFX timeout 30 "$WINESERVER" -w 2>/dev/null; sleep 1; }
start_server
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null; chmod 3770 "$PFX"
other() {
    sudo -n -u "$SG_OTHER" timeout 120 env WINEPREFIX="$PFX" WINEDEBUG=-all WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
        XDG_RUNTIME_DIR="$W/otherhome" \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/otherhome" "$WINE" "$@" 2>/dev/null | tr -d '\r'
}
mode() { stat -c %a "$1" 2>/dev/null; }
other_reads() { sudo -n -u "$SG_OTHER" cat "$1" >/dev/null 2>&1; }

HIVE="$PFX/user-$OUID.reg"
MINE="$PFX/user-$(id -u).reg"
other reg add 'HKCU\Software\SgPrivate' /v Mine /d yes /f >/dev/null
stop_server
[ -f "$HIVE" ] || { echo "FAIL no hive for $SG_OTHER: $(ls "$PFX" | tr '\n' ' ')"; exit 1; }
case "$(mode "$HIVE")" in
600) pass "a person's hive is written 0600" ;;
*)   fail "user-$OUID.reg is $(mode "$HIVE")" ;;
esac
if other_reads "$HIVE"; then fail "another account in the group can read the file (as $SG_OTHER stands in for one)"
else pass "no other account can read the hive file"; fi
[ "$(stat -c %u "$HIVE")" = "$(id -u)" ] && pass "it is the server's file" || fail "owner $(stat -c %U "$HIVE")"

# 2. a hive an older server left 0644
chmod 644 "$HIVE"
[ -f "$MINE" ] && chmod 644 "$MINE"
sysmode=$(mode "$PFX/system.reg"); defmode=$(mode "$PFX/userdef.reg")
before=$(md5sum < "$HIVE")
start_server
out=$(other reg query 'HKCU\Software\SgPrivate' /v Mine)
printf '%s' "$out" | grep -q 'Mine.*yes' && pass "the person reads their HKCU through the server" \
    || fail "own value: '$out'"
[ "$(mode "$HIVE")" = 600 ] && pass "a 0644 hive is closed to others when it is loaded" \
    || fail "after load: $(mode "$HIVE")"
[ "$(md5sum < "$HIVE")" = "$before" ] && pass "without being rewritten" || fail "the hive was rewritten on load"

# 3. the owner's own hive; the machine's files keep their modes
"$WINE" reg query 'HKCU\Software' >/dev/null 2>&1
stop_server
if [ -f "$MINE" ]; then
    [ "$(mode "$MINE")" = 600 ] && pass "the server owner's own hive is closed too" || fail "own hive: $(mode "$MINE")"
fi
[ "$(mode "$PFX/system.reg")" = "$sysmode" ] && [ "$(mode "$PFX/userdef.reg")" = "$defmode" ] \
    && pass "system.reg and userdef.reg keep their modes ($sysmode, $defmode)" \
    || fail "system.reg $(mode "$PFX/system.reg") (was $sysmode), userdef.reg $(mode "$PFX/userdef.reg") (was $defmode)"

# 4. a hive that is a symlink (to a file of the server's): not chmod'ed through it
echo keep > "$W/elsewhere"; chmod 644 "$W/elsewhere"
mv "$HIVE" "$W/realhive"
ln -s "$W/elsewhere" "$HIVE"
start_server
other reg query 'HKCU\Software' >/dev/null
stop_server
[ "$(mode "$W/elsewhere")" = 644 ] && pass "a symlink in place of a hive is not followed" \
    || fail "the link's target became $(mode "$W/elsewhere")"

[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
