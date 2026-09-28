#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SYSTEM's token is elevated; a standard user's is not (patches/sg/0465).
#
# An elevated program runs as SYSTEM (ADR 0012). Its token's elevation type
# was Limited, so TokenElevation said it was not elevated: the .NET SDK's
# installer (WiX Burn) refused to work from its elevated engine ("Elevated
# engine process is not running with elevated privileges") and failed with
# 0x8000ffff. Here the prefix's owner stands for SYSTEM, as on the system.
#
#   WINE=/opt/wine-sg/bin/wine test/sysrelevated-gate.sh
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

T=$(mktemp -d /var/tmp/sg-sysrelevated.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/sysrelevated-probe.exe" "$HERE/sysrelevated-probe.c" || { fail "probe did not build"; exit 1; }
chmod 755 "$T/sysrelevated-probe.exe"
mkdir "$WINEPREFIX"; chgrp "$SG_GROUP" "$WINEPREFIX"; chmod 2770 "$WINEPREFIX"; touch "$WINEPREFIX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$WINEPREFIX" 2>/dev/null
sys=$("$WINE" "$T/sysrelevated-probe.exe" 2>/dev/null | tr -d '\r')
usr=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$WINEPREFIX" WINEDEBUG=-all HOME="$HOME" DISPLAY= \
      "$WINE" "Z:$(printf '%s' "$T/sysrelevated-probe.exe" | tr / '\\')" 2>/dev/null | tr -d '\r')
echo "      SYSTEM: $sys"; echo "      user:   $usr"
[ "$sys" = "elevated 1 type 2" ] && pass "SYSTEM's token is elevated (TokenElevation, type Full)" || fail "SYSTEM: $sys"
[ "$usr" = "elevated 0 type 1" ] && pass "a standard user's is not (type Default: elevation goes through the broker)" || fail "user: $usr"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
