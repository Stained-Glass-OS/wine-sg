#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder whose descriptor gives the users read and execute stays every user's to open (6114).
# CCleaner 7's installer, elevated (SYSTEM = the server's account), sets SYSTEM, Administrators and
# Users (read, execute) on its folders. The Users ACE mapped to the group's bits only; the folder's
# group was the elevated account's own (Program Files is not setgid), so with other::--- no user
# could open the folder: CCleaner did not start for bob, its uninstaller failed with error 5.
# Here: a shared prefix; the server's account makes C:\Program Files\SgUsersRead (and a file) with
# that descriptor and sets it again with SetFileSecurity; sgconf (a second Unix account) lists the
# folder and reads the file, and still cannot write it.
#   WINE=/path/to/wine test/usersread-gate.sh
# Mutant: SG_MUTANT_USERS_READ_GROUP_ONLY (server/file.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null && command -v setfacl >/dev/null || { echo "SKIP: no mingw/setfacl"; exit 77; }
W=$(mktemp -d /var/tmp/usersread.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    sudo -n -u "$SG_OTHER" find "$W" -user "$SG_OTHER" -depth -exec rm -rf {} + 2>/dev/null
    rm -rf "$W"
}
trap cleanup EXIT INT TERM
"$MINGW" -O1 -o "$W/probe.exe" "$HERE/usersread-probe.c" -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
chmod 755 "$W/probe.exe"
mkdir "$PFX"; chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" SG_USERS_GROUP=$SG_GROUP
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p" </dev/null >/dev/null 2>&1
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" </dev/null >/dev/null 2>&1
rm -f "$PFX/dosdevices/z:"
chmod -R g+rwX "$PFX" 2>/dev/null
other() { sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
    HOME=/var/tmp "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'; }
# the elevated account, as on a system: its own primary group, and Program Files not setgid (a
# folder made there gets its maker's group, as Piriform got sgsystem's)
owner() { sh -c "umask 002; WINEPREFIX='$PFX' WINEDEBUG=-all '$WINE' $*" 2>/dev/null </dev/null | tr -d '\r'; }
chmod g-s "$PFX/drive_c/Program Files"
out=$(owner "'$W/probe.exe'" make "'C:\\Program Files\\SgUsersRead'")
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'FILE 0' && pass "SYSTEM makes the folder and its file with SYSTEM, Administrators, Users (read, execute)" || fail "make: $out"
stat -c '      %A %U:%G %n' "$PFX/drive_c/Program Files/SgUsersRead" "$PFX/drive_c/Program Files/SgUsersRead/app.txt"
out=$(other "$W/probe.exe" use 'C:\Program Files\SgUsersRead')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'LIST 1' && pass "a user lists the folder" || fail "list: $out"
printf '%s\n' "$out" | grep -qx 'READ data' && pass "a user reads (and could run) the file" || fail "read: $out"
printf '%s\n' "$out" | grep -qx 'WRITE 0' && fail "a user writes the file (Users have no write)" || pass "a user cannot write it"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
