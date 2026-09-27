#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# kernelbase's AppContainer SID names (patches/sg/0434): AppContainerRegisterSid,
# AppContainerLookupMoniker, AppContainerUnregisterSid, AppContainerFreeMemory.
# Chrome's sandbox binds them when it creates an AppContainer profile and
# CHECK-fails when one is missing: the browser quit about 12 s after starting,
# silently (sandbox::AppContainerBase::CreateProfile, from Chrome's symbols).
# The names are kept in the user's AppContainer\Mappings\<SID> key.
#
#   WINE=/opt/wine-sg/bin/wine test/appcontainer-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-appcontainer.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/appcontainer-probe.exe" "$HERE/appcontainer-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/appcontainer-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has 'EXPORTS 1 1 1 1' && pass "kernelbase exports the four AppContainer SID functions" || fail "exports: $(printf '%s\n' "$out" | head -1)"
has 'REGISTER 00000000' && pass "a SID's moniker is registered" || fail "register: $(printf '%s\n' "$out" | grep REGISTER)"
has 'LOOKUP 00000000 sg.test.appcontainer' && pass "and looked up" || fail "lookup: $(printf '%s\n' "$out" | grep '^LOOKUP ')"
has 'UNREGISTER 00000000' && has 'LOOKUP_AFTER 80070490 (none)' \
    && pass "unregistered, it is not found (ERROR_NOT_FOUND)" || fail "unregister: $(printf '%s\n' "$out" | grep -E 'UNREGISTER|LOOKUP_AFTER' | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
