#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# HKEY_CLASSES_ROOT is HKCU\Software\Classes over HKLM\Software\Classes, and
# the shell honours the user's choices (patches/sg/0178).
#
# On a shared system prefix (the multi-user machine): the prefix owner (an
# administrator) registers the machine's classes; the second Unix user SG_OTHER
# (default sgconf, in SG_GROUP) registers their own and their choices and checks
# the merged view, AssocQueryString, QueryCurrentDefault and ShellExecute; then
# the owner checks none of it leaked to them. Without that user: one prefix,
# one user (no separation checks).
#   WINE=... WINESERVER=... test/hkcr-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/sg-hkcr.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
unset DISPLAY
MULTI=0
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null && MULTI=1
cleanup() { set +e; "$WINESERVER" -k 2>/dev/null; sleep 1; chmod -R u+w "$W" 2>/dev/null; if [ $MULTI = 1 ]; then sudo -n rm -rf "$W"; else rm -rf "$W"; fi; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O2 -o "$W/hkcr-probe.exe" "$HERE/hkcr-probe.c" -lshlwapi -lshell32 -lole32 -luuid -ladvapi32 \
    || { fail "probe did not build"; exit 1; }
chmod 755 "$W/hkcr-probe.exe"
mkdir "$PFX"
if [ $MULTI = 1 ]; then
    chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
    sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
    sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
    chmod -R g+rwX "$PFX" 2>/dev/null
    owner() { sg "$SG_GROUP" -c "umask 002; '$WINE' '$W/hkcr-probe.exe' $*" 2>/dev/null | tr -d '\r'; }
    user() { sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
                 "$WINE" "$W/hkcr-probe.exe" "$@" 2>/dev/null | tr -d '\r'; }
    echo "info  shared prefix: $(id -un) is the administrator, $SG_OTHER the standard user"
else
    timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
    owner() { "$WINE" "$W/hkcr-probe.exe" "$@" 2>/dev/null | tr -d '\r'; }
    user() { owner "$@"; }
    echo "info  no $SG_OTHER: one user, no separation checks"
fi
[ $MULTI = 1 ] || "$WINESERVER" -w 2>/dev/null
m=$(owner machine); printf '%s\n' "$m" | sed 's/^/      admin: /'
u=$(user user); printf '%s\n' "$u" | sed 's/^/      user:  /'
out="$m
$u"
expect() { if printf '%s\n' "$out" | grep -qx -- "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" | head -1 || true))"; fi; }
expect "admin_create_machine=1" "a key created through HKCR by an administrator is the machine's"
expect "admin_create_not_user=1" "and not the user's"
expect "ext_class=SgUser.File" "the user's .ext class hides the machine's"
expect "machine_subkey=1" "a machine subkey the user's key lacks is still there (HKCR\\.ext\\ShellNew)"
expect "relative_subkey=1" "also opened relative to the merged key"
expect "enum_both=11 count=2" "enumeration and RegQueryInfoKey merge both sides' subkeys"
expect 'user_progid="*[^"]*hkcr-probe.exe" mark user "%1"' "a ProgId registered only by the user is in HKCR"
expect "root_has_user_key=1" "HKCR's own enumeration has the user's keys"
expect "root_has_machine_key=1" "and the machine's"
expect "write_went_user=1" "a value written through HKCR to a key the user has goes to the user's"
expect "create_somewhere=1" "a key a standard user creates through HKCR exists afterwards"
expect 'assoc_nochoice=machine "%1"' "AssocQueryString: the class the extension names"
expect 'assoc_choice=choice "%1"' "AssocQueryString: the user's UserChoice wins"
expect 'assoc_protocol=protochoice "%1"' "a URL protocol's UserChoice wins"
expect "current_default=SgChoice.File" "QueryCurrentDefault reports the user's choice"
expect "open_nochoice=machine" "ShellExecute of a file: its class's program"
expect "open_choice=choice" "ShellExecute of a file: the user's chosen program"
expect "open_protocol=protochoice" "ShellExecute of a URL: the user's chosen program"
if [ $MULTI = 1 ]; then
    o=$(owner observe); printf '%s\n' "$o" | sed 's/^/      admin: /'
    out=$o
    expect "other_ext_class=SgMachine.File" "another user sees the machine's class, not this user's"
    expect "other_sees_user_key=0" "and none of this user's keys"
    expect 'other_assoc=machine "%1"' "nor this user's choice"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
