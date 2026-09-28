#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Every user of a shared prefix has the logon variables (patches/sg/0467).
#
# A Windows logon defines APPDATA, LOCALAPPDATA, HOMEDRIVE and HOMEPATH from
# the profile. On a shared prefix wineboot writes them (Volatile Environment)
# only for SYSTEM, the owner, so another user's programs had no %APPDATA%
# or %LOCALAPPDATA%: the .NET SDK's NuGet failed every restore ("Value cannot
# be null. (Parameter 'path1')"). They now come from the profile, as
# USERPROFILE does (0018).
#
#   WINE=/opt/wine-sg/bin/wine test/logonvars-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
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

T=$(mktemp -d /var/tmp/sg-logonvars.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/logonvars-probe.exe" "$HERE/logonvars-probe.c" || { fail "probe did not build"; exit 1; }
chmod 755 "$T/logonvars-probe.exe"
mkdir "$WINEPREFIX"; chgrp "$SG_GROUP" "$WINEPREFIX"; chmod 2770 "$WINEPREFIX"; touch "$WINEPREFIX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$WINEPREFIX" 2>/dev/null
usr=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$WINEPREFIX" WINEDEBUG=-all HOME="$HOME" DISPLAY= \
      "$WINE" "Z:$(printf '%s' "$T/logonvars-probe.exe" | tr / '\\')" 2>/dev/null | tr -d '\r')
echo "$usr" | sed 's/^/      /'
P="C:\\users\\$SG_OTHER"
echo "$usr" | grep -qxF "USERPROFILE=$P" || fail "USERPROFILE: $(echo "$usr" | grep USERPROFILE)"
echo "$usr" | grep -qxF "APPDATA=$P\\AppData\\Roaming" && pass "APPDATA is the user's Roaming folder" || fail "$(echo "$usr" | grep '^APPDATA')"
echo "$usr" | grep -qxF "LOCALAPPDATA=$P\\AppData\\Local" && pass "LOCALAPPDATA is the user's Local folder" || fail "$(echo "$usr" | grep LOCALAPPDATA)"
echo "$usr" | grep -qxF "HOMEDRIVE=C:" && echo "$usr" | grep -qxF "HOMEPATH=\\users\\$SG_OTHER" \
    && pass "HOMEDRIVE and HOMEPATH name the profile" || fail "$(echo "$usr" | grep HOME | tr '\n' ' ')"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
