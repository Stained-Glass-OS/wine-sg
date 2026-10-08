#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What a process is created with is kept and held to (patches/sg/1641),
# 64- and 32-bit: test/childpolicy-probe.c starts itself with
# PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY and CHILD_PROCESS_POLICY (as
# Chromium and Edge start their sandboxed processes): the child reports the
# policies, its parent reads them, and it cannot create a process
# (ERROR_CHILD_PROCESS_BLOCKED) or lift that; a process can restrict itself;
# the component filter and audit attributes are accepted. Both attributes
# were dropped ("Unsupported attribute").
#
#   WINE=/opt/wine-sg/bin/wine test/childpolicy-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_CHILD_PROCESSES_ALLOWED (server/process.c),
# SG_MUTANT_NO_CREATION_POLICY (ntdll/unix/process.c),
# SG_MUTANT_NO_CREATION_ATTRS (kernelbase/process.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-childpolicy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/childpolicy-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/childpolicy-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
