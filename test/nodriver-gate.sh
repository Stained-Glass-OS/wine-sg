#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program listening for audio device changes when no audio driver loads
# (patches/sg/9558): the sound server unreachable, or no driver chosen. 9416's
# mixer watch started anyway, asserted in the missing driver and hung the
# program (Steam stopped right after starting its web helper). test/nodriver-
# probe.c registers an IMMNotificationClient, waits, asks for the default
# output (none: E_NOTFOUND) and must end.
#
#   WINE=/opt/wine-sg/bin/wine test/nodriver-gate.sh
# Mutant (mmdevapi, -DSG_MUTANT_WATCH_WITHOUT_DRIVER).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-nodriver.XXXXXX)
# no audio driver: none of them may load (and none reaches the person's sound server)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d;winepulse.drv,winealsa.drv,wineoss.drv=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/nodriver-probe.exe" "$HERE/nodriver-probe.c" -lole32 -luuid \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
timeout -s KILL 40 "$WINE" "$T/nodriver-probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
grep -q '^register 0$' "$T/o" && pass "a device notification client is registered" || fail "registering: $(tr '\n' ' ' < "$T/o")"
grep -q '^default 0x80070490$' "$T/o" && pass "the program goes on: no default output (E_NOTFOUND), as with no audio device" \
    || fail "the program stopped after registering: $(tr '\n' ' ' < "$T/o")"
grep -q '^DONE$' "$T/o" && pass "and ends" || fail "it never ended"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
