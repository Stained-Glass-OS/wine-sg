#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# powershell.exe hands PowerShell 7 its own standard handles (patch 5930):
# with a stand-in pwsh.exe (test/powershell-stdio-probe.c) at
# %ProgramFiles%\PowerShell\7, "powershell -Command ..." run from a pipe or
# into a file must pass input, output, error and the exit code through.
# Mutant: SG_MUTANT_PS_NO_STDHANDLES (pwsh gets no handles: nothing comes out).
#
#   WINE=/opt/wine-sg/bin/wine test/powershell-stdio-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-psstdio.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/pwsh.exe" "$HERE/powershell-stdio-probe.c" \
    || { echo "FAIL  stand-in did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/Program Files/PowerShell/7"
cp "$T/pwsh.exe" "$WINEPREFIX/drive_c/Program Files/PowerShell/7/pwsh.exe"

# from a pipe into a file, error into another file
printf 'first\nsecond\n' | timeout -s KILL 60 env DISPLAY= "$WINE" powershell -NoProfile -Command "Get-Thing" \
    > "$T/out" 2> "$T/err"
rc=$?
"$WINESERVER" -k 2>/dev/null   # a pwsh left waiting (no handles) must not hang the gate
tr -d '\r' < "$T/out" > "$T/out.txt"; tr -d '\r' < "$T/err" > "$T/err.txt"
grep -qx 'ARGS:-NoProfile -Command Get-Thing' "$T/out.txt" && pass "the command line reaches pwsh, its output reaches a file" \
    || fail "output: $(head -c 300 "$T/out.txt")"
grep -qx 'IN:first' "$T/out.txt" && grep -qx 'IN:second' "$T/out.txt" && pass "standard input from a pipe reaches pwsh" \
    || fail "input not passed"
grep -qx 'ERR:stand-in' "$T/err.txt" && pass "standard error reaches its own file" || fail "error: $(head -c 200 "$T/err.txt")"
[ "$rc" = 7 ] && pass "pwsh's exit code comes back (7)" || fail "exit code $rc"

# into a pipe
mkfifo "$T/fifo"; cat "$T/fifo" > "$T/pipe" &
timeout -s KILL 60 env DISPLAY= "$WINE" powershell -Command "x" </dev/null 2>/dev/null > "$T/fifo"
"$WINESERVER" -k 2>/dev/null; wait
n=$(tr -d '\r' < "$T/pipe" | grep -c '^ARGS:-Command x$')
[ "$n" = 1 ] && pass "output into a pipe" || fail "pipe got $n lines"

echo "powershell-stdio-gate: $fails failure(s)"
[ "$fails" = 0 ]
