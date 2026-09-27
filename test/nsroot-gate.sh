#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0443: the Desktop's shell namespace -- what a file dialog's
# "Desktop" shows -- has no "/" (Wine's view of the Unix file system), which
# Windows does not have; the Unix root stays a drive under This PC, and a
# Unix path typed into a dialog still opens.
#   WINE=... test/nsroot-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/nsroot-gate.XXXXXX)
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
cleanup() { set +e; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; rm -rf "$W"; }
trap cleanup EXIT
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/probe.exe" "$HERE/nsroot-probe.c" -lole32 -lshell32 -lshlwapi || { fail "probe did not build"; exit 1; }
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
"$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" "$W/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
grep -qx 'ITEM /' <<<"$out" && fail "the Desktop lists \"/\"" || pass "the Desktop does not list \"/\""
grep -qx 'ITEM This PC' <<<"$out" && pass "it lists This PC" || fail "no This PC: $out"
grep -qx 'PARSE ok' <<<"$out" && pass "a Unix path still parses" || fail "Unix path: $out"
echo "nsroot-gate: $fails failure(s)"
[ "$fails" = 0 ]
