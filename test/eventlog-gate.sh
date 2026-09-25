#!/bin/bash
# Gate for wine-sg 0143-0145: a real event log.
#
# The Event Log service (wevtsvc) owns the logs; advapi32 is its client. On a
# shared (system) prefix -- this user owns it and is SYSTEM there, the second
# Unix user SG_OTHER (default sgconf, in group SG_GROUP) is a standard user:
#   1. the functional checks, 64-bit and 32-bit (report/read round trip,
#      strings, SID, data, sources, count/oldest, backwards/seek reads,
#      too-small buffers, backups, clearing with a backup),
#   2. the service's own events (6005 in System),
#   3. what a standard user may do: write and read Application, but not
#      read the Security log, write it, clear or back up a log,
#   4. that SYSTEM may read and write Security,
#   5. records survive a wineserver restart,
#   6. a log over its MaxSize loses its oldest records,
#   7. NotifyChangeEventLog.
#   WINE=... WINESERVER=... test/eventlog-gate.sh
# Needs passwordless `sudo -u $SG_OTHER`; exit 77 when that is missing.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW64=${MINGW64:-x86_64-w64-mingw32-gcc}
MINGW32=${MINGW32:-i686-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/eventlog.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW64" >/dev/null && command -v "$MINGW32" >/dev/null || { echo "SKIP: no mingw"; exit 77; }

cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW64" -O2 -municode -o "$W/probe64.exe" "$HERE/eventlog-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
"$MINGW32" -O2 -municode -o "$W/probe32.exe" "$HERE/eventlog-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W"/probe*.exe

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" LC_ALL=en_US.UTF-8
unset DISPLAY
start_server() { sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"; }
start_server
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null

me() { timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
other() {
    sudo -n -u "$SG_OTHER" timeout 120 env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" \
        LC_ALL=en_US.UTF-8 HOME=/var/tmp "$WINE" "$@" 2>/dev/null | tr -d '\r'
}

# a log of our own, with a registered source (as an installer would register one)
me reg add 'HKLM\System\CurrentControlSet\Services\EventLog\SgProbe\SgProbeSrc' /f >/dev/null

# 1. functional checks, both architectures
for arch in 64 32; do
    out=$(me "$W/probe$arch.exe" selftest)
    printf '%s\n' "$out" | sed "s/^/  [$arch] /"
    if printf '%s\n' "$out" | grep -q '^SELFTEST OK'; then pass "$arch-bit: every functional check"
    else fail "$arch-bit: $(printf '%s\n' "$out" | grep -c '^FAIL') functional check(s) failed"; fi
done

# 2. the service's own start event
me "$W/probe64.exe" dump System | grep -q '^REC [0-9]* 4 6005 EventLog ' \
    && pass "System has the service's start event (6005)" || fail "no 6005 event in System"

# 3. a standard user
out=$(other "$W/probe64.exe" report SgProbeUser 4 777 "written by the standard user")
[ "$out" = OK ] && pass "a standard user writes to Application" || fail "a standard user cannot report: $out"
me "$W/probe64.exe" dump Application | grep -q '^REC .* 777 SgProbeUser | written by the standard user |' \
    && pass "...and the event is in Application with its text" || fail "the standard user's event is not in Application"
other "$W/probe64.exe" dump Application | grep -q '777 SgProbeUser' \
    && pass "a standard user reads Application" || fail "a standard user cannot read Application"
out=$(other "$W/probe64.exe" open Security)
[ "$out" = "ERR 5" ] && pass "a standard user may not read Security (ERROR_ACCESS_DENIED)" \
    || fail "a standard user opening Security got '$out'"
out=$(other "$W/probe64.exe" register Security)
[ "$out" = "ERR 5" ] && pass "a standard user may not write Security" || fail "a standard user registering Security got '$out'"
out=$(other "$W/probe64.exe" clear Application)
[ "$out" = "ERR 5" ] && pass "a standard user may not clear a log" || fail "a standard user clearing Application got '$out'"
out=$(other "$W/probe64.exe" backup Application 'C:\users\Public\sg-evl.bak')
[ "$out" = "ERR 1314" ] && pass "a standard user may not back up a log (ERROR_PRIVILEGE_NOT_HELD)" \
    || fail "a standard user backing up Application got '$out'"
out=$(other "$W/probe32.exe" open Security)
[ "$out" = "ERR 5" ] && pass "...nor through a 32-bit program" || fail "32-bit standard user opening Security got '$out'"

# 4. SYSTEM (the prefix owner) and the Security log
out=$(me "$W/probe64.exe" report Security 8 4624 "an audit event")
[ "$out" = OK ] && pass "SYSTEM writes to Security" || fail "SYSTEM reporting to Security got '$out'"
me "$W/probe64.exe" dump Security | grep -q '^REC .* 8 4624 Security | an audit event |' \
    && pass "SYSTEM reads Security" || fail "SYSTEM cannot read its Security event"

# 5. persistence
before=$(me "$W/probe64.exe" count Application)
WINEPREFIX=$PFX "$WINESERVER" -k; sleep 2
start_server
after=$(me "$W/probe64.exe" count Application)
me "$W/probe64.exe" dump Application | grep -q '777 SgProbeUser' && [ -n "$before" ] && [ "$before" = "$after" ] \
    && pass "records survive a wineserver restart ($after)" || fail "after a restart: '$before' became '$after'"
[ -f "$PFX/drive_c/windows/system32/winevt/Logs/Application.sgevt" ] \
    && pass "the log lives in system32\\winevt\\Logs" || fail "no Application.sgevt"

# 6. MaxSize: 64 KB, then 200 events of ~1 KB
me reg add 'HKLM\System\CurrentControlSet\Services\EventLog\SgProbe' /v MaxSize /t REG_DWORD /d 65536 /f >/dev/null
me "$W/probe64.exe" flood SgProbeSrc 200 900 5001 >/dev/null
read -r _ count _ oldest < <(me "$W/probe64.exe" count SgProbe)
size=$(stat -c %s "$PFX/drive_c/windows/system32/winevt/Logs/SgProbe.sgevt" 2>/dev/null || echo 0)
last=$(me "$W/probe64.exe" dump SgProbe | grep '^REC' | tail -1 | awk '{print $4}')
[ "$size" -le 65536 ] && [ "$count" -lt 200 ] && [ "$count" -gt 20 ] && [ "$last" = 5200 ] \
    && pass "over MaxSize the oldest go ($count records kept, file $size bytes, newest 5200)" \
    || fail "MaxSize: $count records, file $size bytes, newest $last"

# 7. notification
( me "$W/probe64.exe" notify Application > "$W/notify.out" ) &
npid=$!
sleep 3
me "$W/probe64.exe" report SgProbeNotify 4 1 "wake up" >/dev/null
wait $npid
grep -q NOTIFIED "$W/notify.out" && pass "NotifyChangeEventLog signals a new event" \
    || fail "NotifyChangeEventLog: $(tr '\n' ' ' < "$W/notify.out")"

echo "eventlog-gate: $fails failure(s)"
[ $fails -eq 0 ]
