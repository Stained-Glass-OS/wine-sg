#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder a user makes in an inheriting shared tree keeps its setgid (6109). In a Stained Glass
# system prefix ProgramData is sgsystem:sgwine 2775 with a default ACL giving the group rwx: what
# a user makes there is the users' group's, and SYSTEM (sgsystem, in sgwine) may replace it, as
# Windows' SYSTEM may. The client's fchmod of a folder it had just made (the server's mode, setgid
# not in it) took the bit off: the next folder down was its maker's own group, its files bob:bob
# 664, and the elevated copy of Epic's launcher could not replace its update data (EC-PRM).
# Here: a shared prefix (the server's account and sgconf, a second Unix account), a tree like
# ProgramData's; sgconf makes Update\Sub\data.bin in it, then the server's account writes over it.
#   WINE=/path/to/wine test/setgidtree-gate.sh
# Mutant: SG_MUTANT_SETGID_DROPPED (dlls/ntdll/unix/file.c).
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
W=$(mktemp -d /var/tmp/setgidtree.XXXXXX)
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
"$MINGW" -O1 -o "$W/probe.exe" "$HERE/setgidtree-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
chmod 755 "$W/probe.exe"
mkdir "$PFX"; chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" SG_USERS_GROUP=$SG_GROUP
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p" </dev/null >/dev/null 2>&1
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" </dev/null >/dev/null 2>&1
rm -f "$PFX/dosdevices/z:"
chmod -R g+rwX "$PFX" 2>/dev/null
# the tree, as a system prefix's ProgramData is: the users' group, setgid, a default ACL with the group's rwx
T=$PFX/drive_c/ProgramData/SgTree
mkdir -p "$T"; chgrp "$SG_GROUP" "$T"; chmod 2775 "$T"
setfacl -d -m u::rwx,g::rwx,o::rx "$T"
other() { sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
    HOME=/var/tmp "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'; }
owner() { sg "$SG_GROUP" -c "umask 002; WINEPREFIX='$PFX' WINEDEBUG=-all '$WINE' $*" 2>/dev/null </dev/null | tr -d '\r'; }
out=$(other "$W/probe.exe" make 'C:\ProgramData\SgTree')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'FILE 0' && pass "the user makes Update\\Sub\\data.bin in the tree" || fail "the user's folders and file: $out"
m=$(stat -c '%A %G' "$T/Update" 2>/dev/null)
case "$m" in d?????s*" $SG_GROUP") pass "Update keeps the tree's setgid and group ($m)";; *) fail "Update: $m";; esac
m=$(stat -c '%G' "$T/Update/Sub/data.bin" 2>/dev/null)
[ "$m" = "$SG_GROUP" ] && pass "what is made below it is the users' group's ($m)" || fail "data.bin's group: $m"
out=$(owner "'$W/probe.exe'" replace "'C:\\ProgramData\\SgTree'")
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'REPLACE 0' && pass "SYSTEM writes over the user's file" || fail "SYSTEM's replace: $out"
printf '%s\n' "$out" | grep -qx 'NEW 0' && pass "SYSTEM makes a file in the user's folder" || fail "SYSTEM's new file: $out"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
