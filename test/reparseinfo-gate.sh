#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A handle opened on a symlink with FILE_FLAG_OPEN_REPARSE_POINT reports
# FILE_ATTRIBUTE_REPARSE_POINT in its FileBasicInfo (patches/sg/0471).
#
# The handle is the link itself, but the attributes came from fstat of the
# file Unix opened -- the link's target -- so they lacked 0x400. Office's
# Click-to-Run (repoman) checks a hard-link/symlink leaf that way before
# replacing it; seeing a plain file, it tried to create the link over it and
# staging failed (0xa0000058, 30088-...).
#
#   WINE=/opt/wine-sg/bin/wine test/reparseinfo-gate.sh
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

T=$(mktemp -d /var/tmp/sg-reparseinfo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/reparseinfo-probe.exe" "$HERE/reparseinfo-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
out=$(timeout -s KILL 120 "$WINE" "$T/reparseinfo-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'link 1' || { fail "the symlink could not be made"; echo "RESULT: FAIL"; exit 1; }
has() { [ $(( 0x$1 & 0x400 )) -ne 0 ]; }
set -- $(printf '%s\n' "$out" | sed -n 's/^link basic \([0-9a-f]*\) handle \([0-9a-f]*\)/\1 \2/p')
has "${1:-0}" && pass "the link's FileBasicInfo says REPARSE_POINT" || fail "the link's FileBasicInfo: ${1:-?}"
has "${2:-0}" && pass "and GetFileInformationByHandle too" || fail "GetFileInformationByHandle: ${2:-?}"
set -- $(printf '%s\n' "$out" | sed -n 's/^target basic \([0-9a-f]*\) handle \([0-9a-f]*\)/\1 \2/p')
has "${1:-0}" && fail "opened without the flag, the target's attributes say REPARSE_POINT (${1})" \
              || pass "opened without the flag, the target's attributes do not"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
