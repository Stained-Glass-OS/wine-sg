#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Symbolic links and junctions are real (patches/sg/0360).
#
# CreateSymbolicLinkW was a stub that reported success and made nothing, so
# installers that lay out a "current" link (the EA app: INST-14-1627, then a
# rollback) failed. The probe makes file and directory symbolic links and a
# junction, reads their reparse data back, follows them, and deletes the
# links without touching their targets.
#
#   WINE=/opt/wine-sg/bin/wine test/symlink-gate.sh
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
T=$(mktemp -d /var/tmp/sg-symlink.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/symlink-probe.exe" "$HERE/symlink-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX" "$T/work"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(cd "$T/work" && timeout -s KILL 120 "$WINE" "$T/symlink-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
n=0
for line in $(printf '%s\n' "$out" | grep -E '^[a-z0-9_]+=[01]$' | grep -v '^failures='); do
    n=$((n + 1))
    case "$line" in
        *=1) pass "${line%=*}" ;;
        *) fail "${line%=*}" ;;
    esac
done
[ "$n" -ge 10 ] || fail "the probe reported only $n checks"
printf '%s\n' "$out" | grep -qx 'failures=0' || fail "probe: $(printf '%s\n' "$out" | grep '^failures=')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
