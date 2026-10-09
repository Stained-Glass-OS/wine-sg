#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WinRT error information and COM process settings (patches/sg/1676), 64-
# and 32-bit: test/roerror-probe.c originates, transforms and matches
# restricted errors (GetRestrictedErrorInfo, SetRestrictedErrorInfo,
# RoGetMatchingRestrictedErrorInfo, RoResolveRestrictedErrorInfoReference,
# a language exception, the reporting flags), registers activation
# factories for the process, calls CoInitializeSecurity twice, and has a
# child process ask for a class object registered with REGCLS_SUSPENDED
# before and after CoResumeClassObjects and CoSuspendClassObjects, passing
# its proxy to CoAllowSetForegroundWindow. These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/roerror-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_RESTRICTED_ERROR, SG_MUTANT_NO_REGISTERED_FACTORIES
# (combase/roapi.c), SG_MUTANT_SECURITY_ALWAYS_OK, SG_MUTANT_NO_RESUME (combase/combase.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-roerror.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/roerror-probe.c" -lruntimeobject -lole32 -loleaut32 -luuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
