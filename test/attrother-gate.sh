#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Clearing the read-only flag of another account's file that has none is not refused
# (patches/sg/7794; David 2026-11-15, GOG GALAXY: its SYSTEM service could not remove a folder the
# client run by him had filled -- "Could not set read-only attribute, error code: 5"). Only a file's
# owner may chmod it, and Wine set the mode even when it did not change. A file the second account
# (sgconf) made in a folder shared with the group: SetFileAttributes(NORMAL) succeeds, the file is
# then deleted.
#
#   WINE=/opt/wine-sg/bin/wine test/attrother-gate.sh   (mutant SG_MUTANT_RO_NOOP_CHMOD, ntdll unix/file.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
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
T=$(mktemp -d /var/tmp/sg-attrother.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; sudo -n -u "$SG_OTHER" rm -rf "$T/shared" 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/attrother-probe.exe" "$HERE/attrother-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
S="$T/shared"; mkdir "$S"; chgrp "$SG_GROUP" "$S"; chmod 2775 "$S"
sudo -n -u "$SG_OTHER" sh -c "umask 002; echo x > '$S/scriptinterpreter.exe'"
echo "      $(stat -c '%A %U:%G' "$S/scriptinterpreter.exe")"
out=$("$WINE" "$T/attrother-probe.exe" "Z:$(printf '%s' "$S/scriptinterpreter.exe" | tr '/' '\\')" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'clear=OK' && pass "clearing a read-only flag the file does not have is not refused" \
    || fail "SetFileAttributes(NORMAL) on another account's file: $(printf '%s' "$out" | grep clear=)"
printf '%s\n' "$out" | grep -qx 'delete=OK' && [ ! -e "$S/scriptinterpreter.exe" ] && pass "and the file is deleted" \
    || fail "delete: $(printf '%s' "$out" | grep delete=)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
