#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Application hives (RegLoadAppKey) are written back as on Windows (wine-sg
# 7807): soon after they change (the lazy writer) and at a normal exit, not
# only when the hive's handle is closed. Visual Studio keeps its private
# registry open for its whole life and is often ended; VsRegEdit sets a value
# and exits with the hive open: both were lost, and Visual Studio's
# first-launch window came back on every start. test/apphive-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/apphive-gate.sh
# Mutants (kernelbase): SG_MUTANT_APPHIVE_NO_EXIT_SAVE (exit: lost),
# SG_MUTANT_APPHIVE_NO_LAZY_SAVE (killed: lost).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-apphive.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/apphive-probe.exe" "$HERE/apphive-probe.c" -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/apphive-probe.exe" "$WINEPREFIX/drive_c/"
run() { (cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" apphive-probe.exe "$@" 2>/dev/null </dev/null | tr -d '\r'); }
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
run set 'C:\exit.bin' >/dev/null
out=$(run get 'C:\exit.bin')
[ "$out" = "get=0 7" ] && pass "a value set by a program that exits with the hive open is in the file (VsRegEdit)" \
    || fail "after a normal exit: '$out'"
run setkill 'C:\killed.bin' >/dev/null
out=$(run get 'C:\killed.bin')
[ "$out" = "get=0 9" ] && pass "and by one killed seconds after the change (the lazy writer; Visual Studio ended)" \
    || fail "after the program was killed: '$out'"
head -1 "$WINEPREFIX/drive_c/exit.bin" 2>/dev/null | grep -q '^WINE REGISTRY Version 2' && pass "the file is a whole hive" || fail "exit.bin: $(head -c 40 "$WINEPREFIX/drive_c/exit.bin" 2>/dev/null)"
ls "$WINEPREFIX/drive_c/"*.sgsave >/dev/null 2>&1 && fail "a save left its temporary file" || pass "no temporary file left behind (written whole, then renamed)"
exit $RC
