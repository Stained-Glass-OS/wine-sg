#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SYSTEM may change the permissions of a file another account owns
# (patches/sg/0466).
#
# The .NET SDK's installer (WiX Burn) downloads into the user's Temp; its
# elevated engine, SYSTEM, moves each payload into C:\ProgramData\Package
# Cache and resets its DACL. The moved file is still the user's on the Unix
# side, the system wineserver runs as sgsystem, and fchmod failed: "Failed to
# reset permissions on unverified cached payload", setup failed 0x8000ffff.
# The system wineserver holds CAP_FOWNER as a permitted capability only
# (sg-session's sg-wineserver.service) and makes it effective for that one
# fchmod. Here a copy of the server is given the capability as the service
# gives it; nothing of it may stay effective.
#
#   WINE=/opt/wine-sg/bin/wine test/sysfowner-gate.sh
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
command -v setcap >/dev/null || [ -x /usr/sbin/setcap ] || [ -x /sbin/setcap ] || { echo "SKIP: no setcap (libcap2-bin)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-sysfowner.XXXXXX); chmod 755 "$T"
cp "$WINESERVER" "$T/wineserver"
sudo -n /usr/sbin/setcap cap_fowner+p "$T/wineserver" 2>/dev/null || sudo -n setcap cap_fowner+p "$T/wineserver" 2>/dev/null \
    || { rm -rf "$T"; echo "SKIP: cannot give the server CAP_FOWNER (sudo setcap)"; exit 77; }
WINESERVER="$T/wineserver"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/sysfowner-probe.exe" "$HERE/sysfowner-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
chmod 755 "$T/sysfowner-probe.exe"
mkdir "$WINEPREFIX"; chgrp "$SG_GROUP" "$WINEPREFIX"; chmod 2770 "$WINEPREFIX"; touch "$WINEPREFIX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$WINEPREFIX" 2>/dev/null
pid=$(pgrep -n -f "^$WINESERVER" || pgrep -n -x wineserver)
eff=$(sed -n 's/^CapEff:[[:space:]]*//p' /proc/$pid/status 2>/dev/null)
amb=$(sed -n 's/^CapAmb:[[:space:]]*//p' /proc/$pid/status 2>/dev/null)
[ "$eff" = "0000000000000000" ] && [ "$amb" = "0000000000000000" ] \
    && pass "the server keeps CAP_FOWNER permitted only (nothing effective or ambient)" || fail "server capabilities: eff $eff amb $amb"
made=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$WINEPREFIX" WINEDEBUG=-all HOME="$HOME" DISPLAY= \
       "$WINE" "Z:$(printf '%s' "$T/sysfowner-probe.exe" | tr / '\\')" make 2>/dev/null | tr -d '\r')
[ "$made" = "made 1" ] || fail "the user could not make the payload: $made"
out=$("$WINE" "$T/sysfowner-probe.exe" 2>/dev/null | tr -d '\r')
echo "$out" | sed 's/^/      /'
echo "$out" | grep -qx "move 1" || fail "SYSTEM could not move the payload"
echo "$out" | grep -qx "dacl-only 0" && pass "SYSTEM resets the DACL of the user's moved file (Burn's cache)" || fail "DACL: $(echo "$out" | grep dacl-only)"
echo "$out" | grep -qx "owner+dacl 0" && pass "with the owner too, as Burn asks per machine" || fail "owner+DACL: $(echo "$out" | grep owner+dacl)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
