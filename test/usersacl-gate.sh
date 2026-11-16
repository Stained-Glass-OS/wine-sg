#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A grant to the machine's users reaches them (patches/sg/0456).
#
# Steam's installer, run as an administrator, gives BUILTIN\Users full
# control of its folder; Steam, run later by a user, writes its updates there.
# Wine ignored the grant -- the folder stayed its maker's -- and Steam
# stopped: "Directory C:\Program Files (x86)\Steam\package not writable". The
# users of the machine are the shared prefix's group: the grant now gives that
# group write, and the folder (setgid) passes the group on to what is made in
# it. Here the prefix's owner makes a folder and grants it; another account
# in the group then writes in it.
#
# And where the folder already has an ACL -- made under a directory whose
# default ACL names the system's account and gives the group r-x (Program
# Files) -- the grant reaches the ACL's group entry, not only its mask
# (patches/sg/7793; David 2026-11-15, GOG GALAXY: "Could not create directory
# ... Dependencies-Temp\__redist"): another user makes folders in it and in a
# folder made there after the grant.
#
#   WINE=/opt/wine-sg/bin/wine test/usersacl-gate.sh   (mutant SG_MUTANT_ACL_GROUP_UNTOUCHED, server/file.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
id -nG | tr ' ' '\n' | grep -qx "$SG_GROUP" || { echo "SKIP: not in $SG_GROUP"; exit 77; }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }

T=$(mktemp -d /var/tmp/sg-usersacl.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
export SG_USERS_GROUP="$SG_GROUP"
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/usersacl-probe.exe" "$HERE/usersacl-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
D="$WINEPREFIX/drive_c/App"
mkdir "$D"; chmod 755 "$D"; chmod o+x "$T" "$WINEPREFIX" "$WINEPREFIX/drive_c"
# what the installer unpacked before it set the grant
mkdir "$D/bin"; chmod 755 "$D/bin"; echo x > "$D/bin/steamui.dll"; chmod 644 "$D/bin/steamui.dll"
out=$("$WINE" "$T/usersacl-probe.exe" 'C:\App' 2>/dev/null | tr -d '\r')
"$WINESERVER" -w
echo "      $out; $(stat -c '%A %U:%G' "$D")"
[ "$out" = "set 0" ] || fail "SetNamedSecurityInfo: $out"
[ "$(stat -c %G "$D")" = "$SG_GROUP" ] && pass "a folder granted to BUILTIN\\Users joins the users' group" || fail "group $(stat -c %G "$D")"
case "$(stat -c %A "$D")" in d???rws*) pass "which may write in it, and passes the group on (setgid)" ;; *) fail "mode $(stat -c %A "$D")" ;; esac
sudo -n -u "$SG_OTHER" mkdir "$D/package" 2>/dev/null && pass "another user makes a folder in it (Steam's package)" || fail "another user cannot write in it"
[ "$(stat -c %G "$D/package" 2>/dev/null)" = "$SG_GROUP" ] && pass "and what is made there is the group's too" || fail "package's group $(stat -c %G "$D/package" 2>/dev/null)"
sudo -n -u "$SG_OTHER" sh -c "echo y > '$D/bin/steamui.dll' && mkdir '$D/bin/new'" 2>/dev/null \
    && pass "and what was there before the grant is theirs too (an update replaces bin/)" || fail "bin/: $(stat -c '%A %G' "$D/bin" "$D/bin/steamui.dll" | tr '\n' ' ')"
# made after the grant by the administrator's program, whose umask is 022
# (Steam as SYSTEM made package\ 0755): the folder's default ACL, not the
# umask, decides -- the users may write in it
(umask 022; mkdir "$D/logs"; : > "$D/logs/bootstrap_log.txt")
sudo -n -u "$SG_OTHER" sh -c "echo z >> '$D/logs/bootstrap_log.txt' && mkdir '$D/logs/sub'" 2>/dev/null \
    && pass "what the administrator's program makes later is the users' to write (inherited, as on Windows)" \
    || fail "made after the grant: $(stat -c '%A %G' "$D/logs" "$D/logs/bootstrap_log.txt" | tr '\n' ' ')"
sudo -n -u "$SG_OTHER" rm -rf "$D/package" "$D/bin/new" "$D/logs/sub" 2>/dev/null

# under a default ACL, as Program Files has (the system's account named, the group r-x)
if command -v setfacl >/dev/null; then
    P="$WINEPREFIX/drive_c/PF"
    mkdir "$P"; chmod 755 "$P"
    setfacl -m "u:$(id -un):rwx" -d -m "u::rwx,u:$(id -un):rwx,g::r-x,o::r-x" "$P"
    mkdir "$P/GOG"
    out=$("$WINE" "$T/usersacl-probe.exe" 'C:\PF\GOG' 2>/dev/null | tr -d '\r')
    "$WINESERVER" -w
    acl=$(getfacl -cp "$P/GOG" 2>/dev/null | grep -E '^(group::|mask::|default:group::)' | tr '\n' ' ')
    echo "      $out; $acl"
    sudo -n -u "$SG_OTHER" mkdir "$P/GOG/Dependencies-Temp" 2>/dev/null \
        && pass "with an inherited ACL, another user makes a folder in it ($acl)" || fail "inherited ACL: another user cannot write ($acl)"
    (umask 022; mkdir "$P/GOG/redist")
    sudo -n -u "$SG_OTHER" mkdir "$P/GOG/redist/__redist" 2>/dev/null \
        && pass "and in a folder made there after the grant" \
        || fail "made after the grant: $(getfacl -cp "$P/GOG/redist" 2>/dev/null | grep -E '^(group::|mask::)' | tr '\n' ' ')"
    sudo -n -u "$SG_OTHER" rm -rf "$P/GOG/Dependencies-Temp" "$P/GOG/redist/__redist" 2>/dev/null
else
    echo "NOTE  no setfacl: the inherited-ACL case skipped"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
