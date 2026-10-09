#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Session notifications (patches/sg/1704), 64- and 32-bit: with a stand-in
# for sg-compositor's control socket SESSION (test/wtsnotify-standin.py;
# sg-compositor's test-wtsevents covers the real one), test/wtsnotify-probe.c
# registers a window with WTSRegisterSessionNotification and is sent
# WM_WTSSESSION_CHANGE for lock, unlock, Remote Desktop taking the session and
# giving it back, and remote control, in order; WTSSessionInfoEx and
# WTSClientProtocolType follow the state. These were stubs and nothing was
# ever sent (Chrome, Edge, Firefox and VS Code register).
#
#   WINE=/opt/wine-sg/bin/wine test/wtsnotify-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_SESSION_EVENTS, SG_MUTANT_NO_LOCK_STATE
# (wtsapi32/wtsapi32.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc python3; do
    command -v $t >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
RC=0; SP=""
T=$(mktemp -d /var/tmp/sg-wtsnotify.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$SP" ] && kill "$SP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/wtsnotify-probe.c" -lwtsapi32 -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    rm -f "$T/ctl.sock" "$T/go"
    python3 "$HERE/wtsnotify-standin.py" "$T/ctl.sock" "$T/go" &
    SP=$!
    n=0; while [ ! -S "$T/ctl.sock" ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
    go="Z:$(printf '%s' "$T/go" | tr / '\\')"
    out=$(cd "$T" && SG_LOCK_CONTROL="$T/ctl.sock" timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe-$a.exe" "$go" \
          2>/dev/null </dev/null | tr -d '\r')
    kill "$SP" 2>/dev/null; SP=""
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
