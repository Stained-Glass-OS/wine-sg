#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Named streams on a volume that has none (patches/sg/0422). Wine's volumes
# do not report FILE_NAMED_STREAMS, and "file:Zone.Identifier" -- which every
# browser writes beside a download -- used to become a second file with a
# colon in its name. Now, as on a FAT volume:
#   - creating or opening "file:stream" fails (ERROR_INVALID_NAME, 123), and
#     nothing appears beside "file"
#   - "file::$DATA" is the file itself
#   - ordinary files are untouched
#
#   WINE=/opt/wine-sg/bin/wine test/streams-gate.sh
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
T=$(mktemp -d /var/tmp/sg-streams.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/streams-probe.exe" "$HERE/streams-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/streams-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has 'NAMED_STREAMS=0' && pass "the volume reports no named streams" || fail "FILE_NAMED_STREAMS reported"
has 'STREAM_CREATE=err 123' && pass "creating file:Zone.Identifier fails, as on FAT" || fail "stream create: $(printf '%s\n' "$out" | grep STREAM_CREATE)"
has 'STREAM_OPEN=err 123' && pass "opening it fails the same way" || fail "stream open: $(printf '%s\n' "$out" | grep STREAM_OPEN)"
has 'ENTRIES=1' && pass "nothing appears beside the file" || fail "directory: $(printf '%s\n' "$out" | grep ENTRIES)"
has 'DATA=hello' && pass "file::\$DATA is the file itself" || fail "::\$DATA: $(printf '%s\n' "$out" | grep DATA=)"
has 'PLAIN_CREATE=ok' && pass "ordinary files are untouched" || fail "plain create"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
