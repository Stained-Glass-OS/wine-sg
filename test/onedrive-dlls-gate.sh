#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cldapi.dll (Cloud Files API) and cryptxml.dll (XML digital signatures),
# patches/sg/0518.  Real Microsoft OneDrive's sync client statically imports
# both: FileSyncClient.dll needs CRYPTXML.dll and FileSyncFALWB.dll needs
# cldapi.dll, so with neither present OneDrive.exe cannot load its sync stack
# ("Library ... not found") and never runs.  This probe drives them as that
# loader does: every export resolves, CfGetPlatformInfo reports the running
# Windows build (a provider reads it to decide the Cloud Filter API is
# present), and the not-yet-implemented placeholder/verify calls fail cleanly
# (E_NOTIMPL) rather than falsely reporting success.
#
#   WINE=/opt/wine-sg/bin/wine test/onedrive-dlls-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-onedrive-dlls.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/onedrive-dlls-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v load)" = "1 1" ] && pass "cldapi.dll and cryptxml.dll load as builtins" \
    || fail "load: $(v load) (expected 1 1)"
[ "$(v cldexports)" = "23 23" ] && pass "all 23 Cloud Files API exports resolve (OneDrive's FileSyncFALWB imports)" \
    || fail "cldapi exports: $(v cldexports) (expected 23 23)"
[ "$(v xmlexports)" = "7 7" ] && pass "all 7 CryptXml exports resolve (OneDrive's FileSyncClient imports)" \
    || fail "cryptxml exports: $(v xmlexports) (expected 7 7)"
set -- $(v platform)
if [ "$1" = "00000000" ] && [ -n "$2" ] && [ "$2" = "$3" ] && [ "$2" != "0" ]; then
    pass "CfGetPlatformInfo reports the running Windows build ($2)"
else
    fail "CfGetPlatformInfo: $(v platform) (expected hr 0 and build == OS build, non-zero)"
fi
[ "$(v xmlclose)" = "00000000" ] && pass "CryptXmlClose of no document succeeds" \
    || fail "xmlclose: $(v xmlclose) (expected 00000000)"
[ "$(v register)" = "80004001" ] && pass "CfRegisterSyncRoot fails cleanly (E_NOTIMPL), not a false success" \
    || fail "register: $(v register) (expected 80004001)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
