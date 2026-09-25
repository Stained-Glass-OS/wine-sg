#!/bin/bash
# Gate for wine-sg 0187: audit events from the Linux side reach the Security
# log. sg-session's PAM hook and elevation broker write each event as a file
# into the audit spool (HKLM\...\EventLog\Security AuditSpool, a Unix
# directory only root and SYSTEM may write); the Event Log service moves them
# into the Security log, and wevtsvc.dll's message table words them for
# Event Viewer.
#
# Shared (system) prefix: this user owns it and is SYSTEM; SG_OTHER (default
# sgconf) is a standard user, who must still be refused the Security log.
#   WINE=... WINESERVER=... test/audit-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW64=${MINGW64:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/audit.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
SPOOL=$W/spool
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW64" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW64" -O2 -municode -o "$W/audit-probe.exe" "$HERE/audit-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/audit-probe.exe"
mkdir -m 0700 "$SPOOL"
mkdir "$PFX"; chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" LC_ALL=en_US.UTF-8
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
"$WINE" reg add 'HKLM\System\CurrentControlSet\Services\EventLog\Security' /v AuditSpool /d "$SPOOL" /f >/dev/null 2>&1
# the service reads AuditSpool when it starts: restart it
"$WINE" net stop eventlog >/dev/null 2>&1; sleep 1

# what sg-audit writes: a logon, an administrator's special privileges, a
# failed logon; a half-written file (a dot name) and a file that is no event
cat > "$SPOOL/1790000000000000001-100.evt" <<EOF
ID 4624
TYPE success
CATEGORY 12544
TIME 1790000000
STRING alice
STRING SGPC
STRING 2 (Interactive)
STRING greetd
STRING tty1
EOF
cat > "$SPOOL/1790000000000000002-100.evt" <<EOF
ID 4672
TYPE success
CATEGORY 12548
STRING alice
STRING SGPC
STRING SeBackupPrivilege, SeRestorePrivilege, SeTakeOwnershipPrivilege, SeDebugPrivilege
STRING greetd
EOF
cat > "$SPOOL/1790000000000000003-101.evt" <<EOF
ID 4625
TYPE failure
CATEGORY 12544
STRING mallory
STRING SGPC
STRING 2 (Interactive)
STRING Unknown user name or bad password.
STRING greetd
STRING tty1
EOF
printf 'ID 4634\nTYPE success\n' > "$SPOOL/.1790000000000000004-100.evt"
printf 'this is not an event\n' > "$SPOOL/1790000000000000005-100.evt"
"$WINE" net start eventlog >/dev/null 2>&1
i=0; while [ -e "$SPOOL/1790000000000000003-101.evt" ] && [ $i -lt 30 ]; do sleep 0.5; i=$((i + 1)); done

left=$(ls -A "$SPOOL" | tr '\n' ' ')
[ "$left" = ".1790000000000000004-100.evt " ] && pass "the spool is emptied, a file still being written (.name) left alone" \
    || fail "the spool holds: '$left'"
"$WINE" "$W/audit-probe.exe" dump 2>/dev/null | tr -d '\r' > "$W/dump.txt"
sed 's/^/      /' "$W/dump.txt"
grep -q '^REC 4624 8 12544 1790000000 Microsoft-Windows-Security-Auditing$' "$W/dump.txt" \
    && pass "4624 is in Security: an audit success, Logon, at the time the spool gave, Microsoft-Windows-Security-Auditing" \
    || fail "no 4624 record as written"
grep -q '^MSG An account was successfully logged on\..*Account Name: *alice.*Logon Type: *2 (Interactive).*Logon Process: *greetd.*Source: *tty1' "$W/dump.txt" \
    && pass "4624's message, worded by wevtsvc.dll with the account and logon type" || fail "4624 message"
grep -q '^CAT Logon' "$W/dump.txt" && pass "the category's name (CategoryMessageFile): Logon" || fail "category name"
grep -q '^REC 4672 8 12548 ' "$W/dump.txt" && grep -q '^MSG Special privileges assigned to new logon\..*SeDebugPrivilege' "$W/dump.txt" \
    && pass "4672 special privileges, Special Logon" || fail "4672"
grep -q '^REC 4625 16 12544 ' "$W/dump.txt" && grep -q '^MSG An account failed to log on\..*mallory.*Unknown user name or bad password' "$W/dump.txt" \
    && pass "4625 is an audit failure with its reason" || fail "4625"
[ "$(grep -c '^REC ' "$W/dump.txt")" = 3 ] && pass "exactly the three events (the half-written and the bad file are not)" \
    || fail "records: $(grep -c '^REC ' "$W/dump.txt")"
o1=$(grep -n '^REC 4624' "$W/dump.txt" | cut -d: -f1); o2=$(grep -n '^REC 4625' "$W/dump.txt" | cut -d: -f1)
[ -n "$o1" ] && [ -n "$o2" ] && [ "$o1" -lt "$o2" ] && pass "in the spool's name order" || fail "order"

# a standard user still may not read them
out=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
      "$WINE" "$W/audit-probe.exe" dump 2>/dev/null | tr -d '\r')
[ "$out" = "ERR 5" ] && pass "a standard user is refused the Security log (5)" || fail "standard user: '$out'"

# the service keeps importing while it runs
printf 'ID 4634\nTYPE success\nCATEGORY 12545\nSTRING alice\nSTRING SGPC\nSTRING 2 (Interactive)\nSTRING greetd\n' > "$SPOOL/.x"
mv "$SPOOL/.x" "$SPOOL/1790000000000000009-100.evt"
i=0; while [ -e "$SPOOL/1790000000000000009-100.evt" ] && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
"$WINE" "$W/audit-probe.exe" dump 2>/dev/null | tr -d '\r' > "$W/dump2.txt"
grep -q '^MSG An account was logged off\..*alice' "$W/dump2.txt" && grep -q '^CAT Logoff' "$W/dump2.txt" \
    && pass "a logoff written while the service runs arrives (4634, Logoff)" || fail "4634 while running"

echo "audit-gate: $fails failure(s)"
[ $fails -eq 0 ]
