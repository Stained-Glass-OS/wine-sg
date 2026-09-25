#!/bin/bash
# Gate for wine-sg 0140: a named pipe server impersonating its client gets the
# CLIENT's token, not its own. Services (the SCM, the event log) decide what a
# caller may do from that token, so a server that impersonated itself would
# grant every standard user the service's own rights.
#
# A shared (system) prefix: this user owns it (and is SYSTEM there); the second
# Unix user SG_OTHER (default sgconf, in group SG_GROUP) is a standard user.
#   WINE=... WINESERVER=... test/pipeimp-gate.sh
# Needs passwordless `sudo -u $SG_OTHER`. Exit 77 when that is missing.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/pipeimp.XXXXXX)
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

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/pipeimp-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
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

run_other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" \
        HOME=/var/tmp "$WINE" "$@"
}

# 1. another user's client
"$WINE" "$W/probe.exe" server sgimp1 > "$W/server1.out" 2>&1 &
spid=$!
sleep 2
run_other "$W/probe.exe" client sgimp1 > "$W/client1.out" 2>&1
wait $spid
tr -d '\r' < "$W/server1.out" > "$W/s1"; tr -d '\r' < "$W/client1.out" > "$W/c1"
cat "$W/s1" "$W/c1"
self=$(sed -n 's/^SERVER //p' "$W/s1")
other=$(sed -n 's/^SELF //p' "$W/c1")
seen=$(sed -n 's/^CLIENT \([^ ]*\).*/\1/p' "$W/s1")
[ -n "$other" ] && [ "$other" != "$self" ] && pass "the two users have different SIDs ($self, $other)" \
    || fail "the second user has no SID of its own ($self, $other)"
[ -n "$seen" ] && [ "$seen" = "$other" ] && pass "impersonating the other user's client gives its SID" \
    || fail "impersonating the other user's client gave $seen, not $other"
grep -q 'CLIENT .* ADMIN 0' "$W/s1" && pass "the standard user's token is not an administrator's" \
    || fail "the impersonated standard user looks like an administrator"

# 2. the same user's client still works
"$WINE" "$W/probe.exe" server sgimp2 > "$W/server2.out" 2>&1 &
spid=$!
sleep 2
"$WINE" "$W/probe.exe" client sgimp2 > "$W/client2.out" 2>&1
wait $spid
seen2=$(tr -d '\r' < "$W/server2.out" | sed -n 's/^CLIENT \([^ ]*\).*/\1/p')
[ "$seen2" = "$self" ] && pass "a client of the server's own user impersonates as that user" \
    || fail "same-user impersonation gave '$seen2'"

echo "pipeimp-gate: $fails failure(s)"
[ $fails -eq 0 ]
