#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Power requests (patches/sg/1678), 64- and 32-bit: on a private session bus
# with a stand-in org.freedesktop.ScreenSaver service (test/powerreq-fake.c),
# test/powerreq-probe.c sets and clears power requests and continuous and
# one-off execution states, and has a child exit holding a request; the
# service's state shows what Wine asked for. These were stubs. (sg-session's
# sg-screensaverd is the real service; sg-compositor's test-power shows its
# hold keeping the idle timers off.)
#
#   WINE=/opt/wine-sg/bin/wine test/powerreq-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_SCREENSAVER_INHIBIT (ntdll/unix/system.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc dbus-run-session cc pkg-config; do
    command -v $t >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
pkg-config --exists dbus-1 || { echo "SKIP: no libdbus-1-dev"; exit 77; }
# everything runs on a bus of its own, never the person's session bus
if [ -z "${SG_POWERREQ_ONBUS:-}" ]; then
    SG_POWERREQ_ONBUS=1 WINESERVER="$WINESERVER" WINE="$WINE" exec timeout -s KILL 900 dbus-run-session -- sh "$0" "$@"
fi
RC=0; FP=""
T=$(mktemp -d /var/tmp/sg-powerreq.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$FP" ] && kill "$FP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/powerreq-probe.c" \
        || { echo "FAIL  probe did not build"; exit 1; }
done
# shellcheck disable=SC2046  # pkg-config's flags are words
cc -O1 $(pkg-config --cflags dbus-1) -o "$T/fake" "$HERE/powerreq-fake.c" $(pkg-config --libs dbus-1) \
    || { echo "FAIL  the stand-in service did not build"; exit 1; }
"$T/fake" "$T/state" &
FP=$!
n=0; while [ ! -s "$T/state" ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
[ -s "$T/state" ] || { echo "FAIL  the stand-in service did not start"; exit 1; }
SG_POWERREQ_STATE="Z:$(printf '%s' "$T/state" | tr / '\\')"
export SG_POWERREQ_STATE
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
