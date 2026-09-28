#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A key granted to the machine's users is theirs to write from a 64-bit
# program's view of the 32-bit registry (patches/sg/0459).
#
# Steam's installer gives BUILTIN\Users full control of
# HKLM\Software\Wow6432Node\Valve\Steam; Steam's client, 64-bit, opens it
# with KEY_WOW64_32KEY. Wine walks such a path a key at a time and asked each
# key on the way -- HKLM itself first -- for the access wanted at the end, so a
# standard user was denied at HKLM: "The Steam registry path is currently not
# writable". The keys on the way are now only passed through, as on Windows.
# Here the prefix's owner makes and grants the key; another account in the
# prefix's group then writes it.
#
#   WINE=/opt/wine-sg/bin/wine test/regwalk-gate.sh
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

T=$(mktemp -d /var/tmp/sg-regwalk.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/regwalk-probe.exe" "$HERE/regwalk-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
chmod 755 "$T/regwalk-probe.exe"
# a shared prefix: its group's members all run in the one wineserver
mkdir "$WINEPREFIX"; chgrp "$SG_GROUP" "$WINEPREFIX"; chmod 2770 "$WINEPREFIX"; touch "$WINEPREFIX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$WINEPREFIX" 2>/dev/null
out=$("$WINE" "$T/regwalk-probe.exe" grant 2>/dev/null | tr -d '\r' | tr '\n' ' ')
[ "$out" = "create 0 setsec 0 " ] && pass "the owner makes the key and grants it to BUILTIN\\Users" || fail "grant: $out"
out=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$WINEPREFIX" WINEDEBUG=-all HOME="$HOME" WINEDLLOVERRIDES="$WINEDLLOVERRIDES" DISPLAY= \
      "$WINE" "Z:$(printf '%s' "$T/regwalk-probe.exe" | tr / '\\')" write 2>/dev/null | tr -d '\r')
echo "$out" | sed 's/^/      /'
echo "$out" | grep -qx "open 0" && echo "$out" | grep -qx "set 0" \
    && pass "another user opens it for writing and writes it (Steam's SteamPID)" || fail "open/set as $SG_OTHER"
echo "$out" | grep -qx "create 0" && pass "and RegCreateKeyEx opens it too" || fail "create as $SG_OTHER"
echo "$out" | grep -qx "parent 5" && pass "HKLM\\Software itself stays shut to them" || fail "parent: $(echo "$out" | grep parent)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
