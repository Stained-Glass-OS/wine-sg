#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Reserving huge inaccessible address space costs no memory (patches/sg/0432).
# Wine keeps a protection byte per page and wrote them all -- zeros for
# PAGE_NOACCESS reservations -- so every 32 GB reserved took 8 MB of resident
# memory; a Chrome renderer (V8's and PartitionAlloc's cages) held over 400 MB
# of them, and Chrome ran machines out of memory.
#
#   - 16 x 32 GB reserved: the process's own memory grows by under 16 MB
#     (stock: 128 MB), and the reservations still work (commit, protection)
#   - released again: no more than that either
#
#   WINE=/opt/wine-sg/bin/wine test/bigreserve-gate.sh
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
T=$(mktemp -d /var/tmp/sg-bigreserve.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/bigreserve-probe.exe" "$HERE/bigreserve-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/bigreserve-probe.exe" "$WINEPREFIX/drive_c/"

private_mb() {
    _pid=$(pgrep -f '^C:.bigreserve-probe.exe' | head -1)
    [ -n "$_pid" ] || { echo -1; return; }
    awk '/^Private_(Clean|Dirty):/ { kb += $2 } END { print int(kb / 1024) }' "/proc/$_pid/smaps_rollup"
}
wait_for() { for i in $(seq 1 60); do grep -q "^$1" "$T/out.txt" 2>/dev/null && return 0; sleep 0.5; done; return 1; }

"$WINE" 'C:\bigreserve-probe.exe' > "$T/out.txt" 2>/dev/null &
wait_for START; sleep 1; before=$(private_mb)
wait_for PROTECT_OK; sleep 1; reserved=$(private_mb)
wait_for RELEASED; sleep 1; released=$(private_mb)
wait
sed 's/^/      /' "$T/out.txt"
echo "      private memory: ${before} MB before, ${reserved} MB with 512 GB reserved, ${released} MB after release"

grep -q '^RESERVED 16' "$T/out.txt" && grep -q '^COMMITTED 42' "$T/out.txt" && grep -q '^PROTECT_OK 1' "$T/out.txt" \
    && pass "the reservations work (16 x 32 GB, a page committed and used, the rest still inaccessible)" \
    || fail "reservations: $(tr '\n' ' ' < "$T/out.txt")"
[ "$before" -ge 0 ] && [ $((reserved - before)) -lt 16 ] \
    && pass "reserving 512 GB costs under 16 MB (+$((reserved - before)) MB)" || fail "reserving cost $((reserved - before)) MB"
[ "$before" -ge 0 ] && [ $((released - before)) -lt 16 ] \
    && pass "and releasing it leaves under 16 MB (+$((released - before)) MB)" || fail "after release: +$((released - before)) MB"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
