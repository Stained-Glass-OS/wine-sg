#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Exports that had no code, batch 1 (patches/sg/8009-8014; test/nocode1-probe.c, test/nocode1-filter.c).
# Spec stubs before (a call raised an exception), all imported by open-source Store apps (census of
# gaps10, rechecked 2026-11-27): ktmw32 transaction queries and async commit/rollback; secur32 LSA policy
# change notifications; netapi32 the global group "None", group writes, messenger calls, workstation
# transports; setupapi/cfgmgr32 resource descriptors; msports the COM port database; query
# BindIFilterFromStorage/Stream (with a test filter DLL registered for .sgflt and a document class).
#   WINE=/opt/wine-sg/bin/wine test/nocode1-gate.sh
# Mutants: SG_MUTANT_NO_POLICY_NOTIFY (advapi32/lsa.c), SG_MUTANT_NO_NONE_GROUP, SG_MUTANT_NO_MESSENGER
# (netapi32), SG_MUTANT_NO_RESDES (setupapi/devinst.c), SG_MUTANT_NO_COMDB_CLAIM (msports),
# SG_MUTANT_NO_BINDIFILTER (query); ktmw32 is caught by a spec stub (8009 left out).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-nocode1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/nocode1-probe.c" \
    -lktmw32 -lnetapi32 -lsetupapi -lole32 -luuid -lshlwapi -ladvapi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -shared -o "$T/sgfilter.dll" "$HERE/nocode1-filter.c" -lole32 -luuid 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the test filter did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
cd "$T" || exit 1
timeout -s KILL 120 "$WINE" "$T/probe.exe" sgfilter.dll >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
