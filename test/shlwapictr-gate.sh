#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SHLWAPI's ordinal-only exports (patches/sg/7703). Programs import many SHLWAPI functions by
# ordinal; Wine numbered its named exports from the first free ordinal, so Windows' ordinals
# 552-645 were other functions (Teams' SHGlobalCounterIncrement, 633, called
# PathIsUNCServerShareW with the counter's number and crashed at start). Now no named export sits
# there, and the global counters by number (632-634) count, shared by every process.
#
#   WINE=/opt/wine-sg/bin/wine test/shlwapictr-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shlwapictr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/p64.exe" "$HERE/shlwapictr-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW32" -O2 -o "$T/p32.exe" "$HERE/shlwapictr-probe.c" 2>/dev/null || true
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"; cp "$T"/p*.exe "$C/"
run() { (cd "$C" && timeout -s KILL 60 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
for b in 64 32; do
    [ -f "$C/p$b.exe" ] || continue
    out=$(run "p$b.exe" ordinals)
    expect "$out" NamedOnWindowsOrdinals 0 "$b-bit: no named export sits on Windows' ordinals 552-645"
    expect "$out" Ordinal633IsPathIsUNCServerShareW 0 "$b-bit: ordinal 633 is not PathIsUNCServerShareW"
    expect "$out" Ordinal633 present "$b-bit: ordinal 633 is exported"
done
n=$$
expect "$(run p64.exe get $((n % 200)))" Value 0 "a global counter starts at 0"
expect "$(run p64.exe inc $((n % 200)))" Value 1 "SHGlobalCounterIncrement counts up (1)"
expect "$(run p64.exe seq $((n % 200 + 1)))" Seq "1 2 2 1 0 0" "increments, the value and decrements follow each other, never below 0"
# shared by the processes of the session: another process sees what one holding it counted
(cd "$C" && timeout -s KILL 60 "$WINE" p64.exe hold $((n % 200 + 2)) 8 >/dev/null 2>&1 &)
python3 -c "import time; time.sleep(4)"
expect "$(run p64.exe get $((n % 200 + 2)))" Value 1 "another process sees the counter"
[ "$RC" = 0 ] && echo "shlwapictr gate: PASS" || echo "shlwapictr gate: FAIL"
exit "$RC"
