#!/bin/sh
# A pipe end reports how much it can write (patches/sg/0083).
#
# FilePipeLocalInformation.WriteQuotaAvailable was always 0 in Wine. The
# Cygwin/MSYS runtime waits for it to cover a write before writing to a pty,
# so no program in Git Bash's terminal could ever print anything.
#
#   WINE=/opt/wine-sg/bin/wine test/pipequota-gate.sh
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
T=$(mktemp -d /var/tmp/sg-pipequota.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/pipequota-probe.exe" "$HERE/pipequota-probe.c" -lntdll || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/pipequota-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'empty server=200 client=100' && pass "an empty pipe: each end can write the other's whole buffer (200, 100)" || fail "empty: $(printf '%s\n' "$out" | grep empty)"
printf '%s\n' "$out" | grep -qx 'written server=170 client=90' && pass "less what it has written and the other end has not read" || fail "after writes: $(printf '%s\n' "$out" | grep written)"
printf '%s\n' "$out" | grep -qx 'read server=200 client=100' && pass "and back when it is read" || fail "after reads: $(printf '%s\n' "$out" | grep '^read')"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
