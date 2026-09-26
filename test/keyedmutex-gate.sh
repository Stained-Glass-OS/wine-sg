#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IDXGIKeyedMutex with Wine's own d3d11 (patches/sg/0424): a texture created
# D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX answers for it -- browsers and video
# players ask for one on the WineD3D path and gave up. Acquire with the key
# it was released with; an owned or wrongly keyed acquire times out; a waiter
# on another thread wakes when the right key is released; a texture without
# the flag has none.
#
#   WINE=/opt/wine-sg/bin/wine test/keyedmutex-gate.sh     (needs Xvfb)
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
command -v Xvfb >/dev/null || { echo "SKIP: no Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-keyedmutex.XXXXXX)
# A display of our own -- never the desktop's.
DN=$(( 400 + $$ % 200 ))
Xvfb ":$DN" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
XPID=$!
export DISPLAY=":$DN" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;d3d11,dxgi=b"
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
sleep 2
"$MINGW" -O2 -o "$T/keyedmutex-probe.exe" "$HERE/keyedmutex-probe.c" -ld3d11 -ldxgi -ldxguid -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/keyedmutex-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has 'CREATE ok' || { fail "no texture: $out"; echo "RESULT: FAIL"; exit 1; }
has 'QI 00000000' && pass "the texture has an IDXGIKeyedMutex" || fail "QI: $(printf '%s\n' "$out" | grep '^QI')"
has 'ACQUIRE0 00000000' && pass "acquired with key 0 at creation" || fail "first acquire"
has 'ACQUIRE_OWNED 00000102' && pass "acquiring it while owned times out (WAIT_TIMEOUT)" || fail "owned acquire"
has 'WAITING 0' && has 'WOKE 1' && pass "a waiter on another thread wakes when key 7 is released" || fail "waiter"
has 'ACQUIRE_WRONGKEY 00000102' && pass "the wrong key times out" || fail "wrong key"
has 'ACQUIRE0_AGAIN 00000000' && pass "released back with 0, acquired with 0" || fail "reacquire"
has 'BACK 1' && pass "QueryInterface leads back to the texture" || fail "QI back"
has 'PLAIN_QI 80004002' && pass "a texture without the flag has none (E_NOINTERFACE)" || fail "plain: $(printf '%s\n' "$out" | grep PLAIN_QI)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
