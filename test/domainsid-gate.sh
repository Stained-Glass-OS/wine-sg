#!/bin/bash
# Gate for wine-sg 0205: a domain account's real SID.
#
# On a shared (system) prefix -- this user owns it and is SYSTEM there; the
# second Unix user SG_OTHER (default sgconf, in group SG_GROUP) plays a domain
# user whose identity sg-domain-logon recorded at sign-in in
# /run/stained-glass/domain-sids/<uid> (made here with sudo, with a made-up
# domain SID, and removed afterwards):
#   1. a record the user could have written (their own file) is ignored:
#      the local SID S-1-5-21-0-0-0-<1000+uid>
#   2. a root-owned record: after the server restarts, the token's user is the
#      domain SID, its primary group the domain's Domain Users and it holds
#      its domain groups; LookupAccountSid names it DOMAIN\name (and the
#      groups), GetUserNameEx(NameSamCompatible) too; LookupAccountName maps
#      DOMAIN\name, name and DOMAIN\group back; a file the user creates is
#      owned by the domain SID; HKCU is HKEY_USERS\<domain SID>, with the
#      settings written while the user had the local SID; an ACL names
#      domain groups (one the user holds, one it does not), reads back by
#      name, and is enforced by them
#   3. another user (SYSTEM) resolves the domain account both ways
#   WINE=... WINESERVER=... test/domainsid-gate.sh
# Needs passwordless sudo (root, and -u $SG_OTHER); exit 77 when missing.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW64=${MINGW64:-x86_64-w64-mingw32-gcc}
DIR=/run/stained-glass/domain-sids
DOMSID=S-1-5-21-3623811015-3361044348-30300820
W=$(mktemp -d /var/tmp/domainsid.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null && sudo -n true 2>/dev/null \
    || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW64" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
OUID=$(id -u "$SG_OTHER")
MADE_RUN=0; [ -d /run/stained-glass ] || MADE_RUN=1
[ -e "$DIR/$OUID" ] && { echo "SKIP: $DIR/$OUID exists (a real sign-in?)"; exit 77; }

cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    sudo -n rm -f "$DIR/$OUID"
    sudo -n rmdir "$DIR" 2>/dev/null
    [ "$MADE_RUN" = 1 ] && sudo -n rmdir /run/stained-glass 2>/dev/null
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW64" -O2 -o "$W/probe.exe" "$HERE/domainsid-probe.c" -ladvapi32 -lsecur32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/probe.exe"
mkdir -m 1777 "$W/files"

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
expect() {   # expect OUTPUT LINE WHAT
    if printf '%s\n' "$1" | grep -qxF -- "$2"; then pass "$3"
    else fail "$3: wanted '$2', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}

record() {   # the record sg-domain-logon writes
    printf 'user %s-1104 TESTDOM\\%s\ngroup %s-513 TESTDOM\\Domain Users\ngroup %s-1110 TESTDOM\\Ünïcode Crew\ngroup %s-512 TESTDOM\\Domain Admins\nname %s-519 TESTDOM\\Enterprise Admins\n' \
        "$DOMSID" "$SG_OTHER" "$DOMSID" "$DOMSID" "$DOMSID" "$DOMSID"
}
sudo -n mkdir -p "$DIR" && sudo -n chmod 755 /run/stained-glass "$DIR"

# 1. a record the user could have written is not believed
record | sudo -n tee "$DIR/$OUID" >/dev/null
sudo -n chown "$SG_OTHER" "$DIR/$OUID"
out=$(other "$W/probe.exe" 'C:\')
expect "$out" "UserSid=S-1-5-21-0-0-0-$((1000 + OUID))" "a record the user owns is ignored: the local SID"
expect "$out" "HkcuIsHkuSid=1" "and HKCU is under it"

# 2. root's record; the server restarts (as a machine does)
sudo -n chown root:root "$DIR/$OUID" && sudo -n chmod 644 "$DIR/$OUID"
WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; sleep 1
start_server
out=$(other "$W/probe.exe" 'Z:'"$(echo "$W/files" | tr / '\\')" "TESTDOM\\$SG_OTHER" "$SG_OTHER" 'TESTDOM\Domain Users' 'testdom\domain admins')
printf '%s\n' "$out" | sed 's/^/  /'
expect "$out" "UserSid=$DOMSID-1104" "the token's user is the domain SID"
expect "$out" "UserName=TESTDOM\\$SG_OTHER use=1" "LookupAccountSid: TESTDOM\\$SG_OTHER, a user"
expect "$out" "SamCompatible=TESTDOM\\$SG_OTHER" "GetUserNameEx(NameSamCompatible): TESTDOM\\$SG_OTHER"
expect "$out" "PrimaryGroup=$DOMSID-513" "the primary group is the domain's Domain Users"
expect "$out" "PrimaryGroupName=TESTDOM\\Domain Users use=2" "named, as a group"
expect "$out" "Group=$DOMSID-512" "the token holds the domain groups (Domain Admins)"
expect "$out" "Group=$DOMSID-1110" "(and one with a non-ASCII name)"
expect "$out" "Group=S-1-5-32-545" "and still the local Users group"
if printf '%s\n' "$out" | grep -qx 'Group=S-1-5-32-544'; then fail "a domain user is not an administrator here: $out"
else pass "and not Administrators (elevation is the broker's)"; fi
expect "$out" "Name[TESTDOM\\$SG_OTHER]=$DOMSID-1104 TESTDOM use=1" "LookupAccountName(TESTDOM\\$SG_OTHER)"
expect "$out" "Name[$SG_OTHER]=$DOMSID-1104 TESTDOM use=1" "LookupAccountName($SG_OTHER): the domain account"
expect "$out" "Name[TESTDOM\\Domain Users]=$DOMSID-513 TESTDOM use=2" "LookupAccountName(TESTDOM\\Domain Users)"
expect "$out" "Name[testdom\\domain admins]=$DOMSID-512 TESTDOM use=2" "names are case-insensitive"
expect "$out" "FileOwner=$DOMSID-1104" "a file the user creates is owned by the domain SID"
expect "$out" "FileGroup=$DOMSID-513" "and its group is Domain Users"
expect "$out" "HkcuIsHkuSid=1" "HKCU is HKEY_USERS\\<domain SID>"
expect "$out" "Earlier=7" "with the settings written under the local SID"
if printf '%s\n' "$out" | grep -q "^Group=$DOMSID-519\$"; then fail "a name-only group is held: $out"
else pass "a name-only group (Enterprise Admins) is not in the token"; fi
# ACLs: a key readable only by a domain group
out=$(other "$W/probe.exe" acl held 'TESTDOM\Domain Users')
expect "$out" "Ace=$DOMSID-513" "an ACL grants a domain group by name (SetEntriesInAcl, SetSecurityInfo)"
expect "$out" "AceName=TESTDOM\\Domain Users use=2" "and reads back with its name"
expect "$out" "Opened=0" "the user, in that group, may open it"
out=$(other "$W/probe.exe" acl notheld 'TESTDOM\Enterprise Admins')
expect "$out" "Ace=$DOMSID-519" "a domain group the user is not in (a name-only entry) can be named in an ACL"
expect "$out" "Opened=5" "and the user may not open what only it may"

# a key the domain user makes in shared HKLM state (as display setup does
# in Enum\DISPLAY) is owned by the machine's Users group, which every
# account holds -- not by the domain's Domain Users, which the greeter and
# local accounts do not, and which left them unable ever to delete it
cat > "$W/share.c" <<'EOF2'
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
int main( void )
{
    PSECURITY_DESCRIPTOR sd;
    HKEY key;
    LONG err;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA( "D:(A;CI;KA;;;SY)(A;CI;KA;;;BA)(A;CI;KA;;;BU)",
                                                              SDDL_REVISION_1, &sd, NULL )) return 1;
    if (!(err = RegCreateKeyExA( HKEY_LOCAL_MACHINE, "Software\\StainedGlassShared", 0, NULL,
                                 REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL )))
        err = RegSetKeySecurity( key, DACL_SECURITY_INFORMATION, sd );
    printf( "Shared=%ld\n", err );
    return 0;
}
EOF2
"$MINGW64" -O2 -o "$W/share.exe" "$W/share.c" -ladvapi32 && chmod 755 "$W/share.exe"
out=$(me "$W/share.exe")
expect "$out" "Shared=0" "(a shared HKLM key that Users may write)"
out=$(other "$W/probe.exe" mkkey 'Software\StainedGlassShared\ByDomainUser')
expect "$out" "KeyOwner=S-1-5-21-0-0-0-513" "a key the domain user creates in HKLM is owned by the machine's Users group"
out=$(other "$W/probe.exe" rmkey 'Software\StainedGlassShared\ByDomainUser')
expect "$out" "Deleted=0" "and its creator may delete it"

# 3. anyone else resolves it
out=$(me "$W/probe.exe" '' "TESTDOM\\$SG_OTHER")
expect "$out" "Name[TESTDOM\\$SG_OTHER]=$DOMSID-1104 TESTDOM use=1" "SYSTEM resolves the domain account's name"
cat > "$W/sidname.c" <<'EOF'
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>
int main( int argc, char **argv )
{
    PSID sid; char name[256], dom[256]; DWORD nl = 256, dl = 256; SID_NAME_USE use;
    if (!ConvertStringSidToSidA( argv[1], &sid )) return 1;
    if (LookupAccountSidA( NULL, sid, name, &nl, dom, &dl, &use )) printf( "Sid=%s\\%s use=%d\n", dom, name, use );
    else printf( "Sid=error %lu\n", GetLastError() );
    return 0;
}
EOF
"$MINGW64" -O2 -o "$W/sidname.exe" "$W/sidname.c" -ladvapi32 && chmod 755 "$W/sidname.exe"
out=$(me "$W/sidname.exe" "$DOMSID-1104")
expect "$out" "Sid=TESTDOM\\$SG_OTHER use=1" "and its SID"
out=$(me "$W/sidname.exe" "$DOMSID-9999")
expect "$out" "Sid=error 1332" "an unknown domain SID is ERROR_NONE_MAPPED"

echo
if [ "$fails" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL ($fails)"; fi
exit $((fails > 0))
