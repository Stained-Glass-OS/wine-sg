#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A registry file the server did not write is not loaded (patches/sg/1475).
# In a prefix shared between users every person may make files in the prefix
# (Wine's clients must), so a person could plant user-<uid>.reg for another
# person before that person first signs in -- their HKCU, with Run keys and
# file associations -- or replace a hive while the server was not running.
# The second Unix user SG_OTHER (default sgconf, in SG_GROUP) plants its own
# hive before it ever connects, standing in for one planted by someone else:
#   1. the planted value is not there; the file was moved aside (.untrusted.*)
#   2. what the user then writes is kept across a server restart (the hive the
#      server writes is its own and is loaded)
#   3. a hive that is a symlink is not followed either
#   WINE=... WINESERVER=... test/hivetrust-gate.sh
# Mutant: SG_MUTANT_TRUST_ANY_HIVE (server/registry.c). Needs passwordless
# sudo -u $SG_OTHER; exit 77 otherwise.
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no sudo -u $SG_OTHER"; exit 77; }
OUID=$(id -u "$SG_OTHER")
W=$(mktemp -d /var/tmp/hivetrust.XXXXXX)
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
unset DISPLAY WAYLAND_DISPLAY
start_server() { sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p" 2>>"$W/server.log"; }
start_server
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null; chmod 3770 "$PFX"
other() {
    sudo -n -u "$SG_OTHER" timeout 120 env WINEPREFIX="$PFX" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/otherhome" "$WINE" "$@" 2>/dev/null | tr -d '\r'
}
restart() { WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; sleep 1; start_server; }

HIVE="$PFX/user-$OUID.reg"
[ ! -e "$HIVE" ] || { echo "FAIL $HIVE exists before $SG_OTHER connected"; exit 1; }
sudo -n -u "$SG_OTHER" sh -c "printf 'WINE REGISTRY Version 2\n#arch=win64\n\n[Software\\\\\\\\Planted] 0\n\"Evil\"=\"1\"\n' > '$HIVE'"
grep -q Planted "$HIVE" && [ "$(stat -c %U "$HIVE")" = "$SG_OTHER" ] || { echo "FAIL could not plant the hive"; exit 1; }
restart
out=$(other reg query 'HKCU\Software\Planted' /v Evil)
if printf '%s' "$out" | grep -q Evil; then fail "the planted hive was loaded: $out"
else pass "a hive the server did not write is not loaded (no planted value)"; fi
ls -a "$PFX" | grep -q "^\.untrusted\.[0-9]*\.user-$OUID\.reg\$" && pass "it was moved aside (.untrusted.*)" \
    || fail "not moved aside: $(ls -a "$PFX" | tr '\n' ' ')"
grep -q "user-$OUID.reg is not the system's" "$W/server.log" && pass "and the server said so" || fail "server log: $(tail -2 "$W/server.log")"

other reg add 'HKCU\Software\Mine' /v Ok /d yes /f >/dev/null
restart
out=$(other reg query 'HKCU\Software\Mine' /v Ok)
printf '%s' "$out" | grep -q 'Ok.*yes' && [ "$(stat -c %u "$HIVE" 2>/dev/null)" = "$(id -u)" ] \
    && pass "what the user writes is kept across a restart (the server's own hive file)" \
    || fail "own value after restart: '$out', hive owner $(stat -c %U "$HIVE" 2>/dev/null)"

# a symlink in place of a hive
WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; sleep 1
mv "$HIVE" "$W/realhive"; chmod 666 "$W/realhive"
sudo -n -u "$SG_OTHER" ln -s "$W/realhive" "$HIVE"
start_server
out=$(other reg query 'HKCU\Software\Mine' /v Ok)
if printf '%s' "$out" | grep -q 'Ok.*yes'; then fail "a hive that is a symlink was followed"
else pass "a hive that is a symlink is not followed"; fi

[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
