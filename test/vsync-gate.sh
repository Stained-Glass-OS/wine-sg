#!/bin/sh
# DwmFlush waits for the next vertical blank (patches/sg/0170).
#
# Wine's DwmFlush returned at once. Firefox paces its vsync thread with it,
# so its vsync notifications -- IPC messages to the GPU process -- became a
# flood: synchronous requests to the GPU process timed out, the browser's
# queued messages grew to tens of GB, and it never shut down. The probe
# checks DwmFlush's pace and runs Firefox's pattern (vsync over an
# overlapped named pipe, an I/O completion port and a posted-message wake).
#
#   WINE=/opt/wine-sg/bin/wine test/vsync-gate.sh
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
T=$(mktemp -d /var/tmp/sg-vsync.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/vsync-probe.exe" "$HERE/vsync-probe.c" -ldwmapi || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/vsync-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/.*$1=\([0-9.-]*\).*/\1/p" | head -1; }
rate=$(v rate); ms=$(v flush30_ms); exp=$(v expect); onv=$(printf '%s\n' "$out" | sed -n 's/^on_vblank=\([0-9]*\)\/30/\1/p')
ps=$(v per_s); rt=$(v roundtrip_ms)
[ -n "$rate" ] && [ "$rate" -gt 0 ] && pass "a refresh rate ($rate Hz)" || fail "no refresh rate"
if [ -n "$ms" ] && [ -n "$exp" ] && [ "$ms" -ge $((exp * 3 / 4)) ] && [ "$ms" -le $((exp * 3 / 2 + 50)) ]; then
    pass "30 DwmFlush calls take 30 frames (${ms} ms, ${exp} expected)"
else fail "30 DwmFlush calls took ${ms:-?} ms, not ~${exp:-?}"; fi
[ -n "$onv" ] && [ "$onv" -ge 25 ] && pass "each returns just after a vertical blank ($onv/30)" || fail "returns off the vertical blank (${onv:-?}/30)"
if [ -n "$ps" ] && [ -n "$rate" ] && [ "$ps" -le $((rate * 3 / 2 + 5)) ] && [ "$ps" -ge $((rate / 2)) ]; then
    pass "Firefox's vsync over pipe + IOCP + posted message runs at the refresh rate ($ps/s)"
else fail "Firefox's vsync pattern ran at ${ps:-?}/s, not ~$rate/s"; fi
case "$rt" in ''|-*) fail "a request through the same channel got no reply" ;;
    *) [ "$rt" -le 200 ] && pass "a request through the same channel is answered (${rt} ms)" || fail "request took ${rt} ms" ;; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
