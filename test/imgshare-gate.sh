#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Image pages are shared between processes even when a DLL's sections are
# not page-aligned in the file (patches/sg/0431). Chromium's and Electron's
# DLLs (chrome.dll: 289 MB) use 512-byte file alignment; Wine read such
# sections into each process's private memory, so every Chrome process
# started with ~300 MB of its own and Chrome ran a machine out of memory.
#
#   - two processes load a DLL with a 32 MB read-only blob, 512-byte aligned:
#     every byte is right, and each process's private memory in the DLL's
#     range stays far below the blob's size (the pages are shared)
#   - its writable data is still each process's own (copy-on-write)
#
#   WINE=/opt/wine-sg/bin/wine test/imgshare-gate.sh
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
T=$(mktemp -d /var/tmp/sg-imgshare.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

python3 -c "import sys; sys.stdout.buffer.write(bytes(((i * 7 + (i >> 12)) & 0xff) for i in range(32 << 20)))" > "$T/blob.bin"
"$MINGW" -O2 -shared -DBLOB_FILE="\"$T/blob.bin\"" -o "$T/imgshare.dll" "$HERE/imgshare-dll.c" &&
    "$MINGW" -O2 -o "$T/imgshare-probe.exe" "$HERE/imgshare-probe.c" || { fail "probe did not build"; exit 1; }
align=$(x86_64-w64-mingw32-objdump -p "$T/imgshare.dll" | awk '/FileAlignment/ {print $2}')
[ "$align" = 00000200 ] || { fail "the test DLL is not 512-byte aligned ($align)"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/imgshare.dll" "$T/imgshare-probe.exe" "$C/"

"$WINE" 'C:\imgshare-probe.exe' A write > "$T/a.txt" 2>/dev/null &
for i in $(seq 1 60); do grep -q '^WDATA ' "$T/a.txt" 2>/dev/null && break; sleep 1; done
"$WINE" 'C:\imgshare-probe.exe' B > "$T/b.txt" 2>/dev/null &
for i in $(seq 1 60); do grep -q '^WDATA ' "$T/b.txt" 2>/dev/null && break; sleep 1; done
sleep 1

# each process's private memory in the DLL's address range
private_kb() {
    _pid=$(pgrep -f "^C:.imgshare-probe.exe $1" | head -1)
    _base=$(awk '/^MAPPED/ {print $2}' "$T/$2.txt"); _size=$(awk '/^MAPPED/ {print $3}' "$T/$2.txt")
    [ -n "$_pid" ] && [ -n "$_base" ] || { echo -1; return; }
    python3 - "$_pid" "$_base" "$_size" <<'PY'
import sys
pid, base, size = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
kb, inr = 0, False
for line in open("/proc/%s/smaps" % pid):
    f = line.split()
    if "-" in f[0] and len(f[0].split("-")) == 2 and all(c in "0123456789abcdef-" for c in f[0]):
        s, e = (int(x, 16) for x in f[0].split("-"))
        inr = e > base and s < base + size
    elif inr and f[0] in ("Private_Clean:", "Private_Dirty:"):
        kb += int(f[1])
print(kb)
PY
}
pa=$(private_kb A a); pb=$(private_kb B b)
wait
cat "$T/a.txt" "$T/b.txt" | sed 's/^/      /'
echo "      private in the DLL's range: A ${pa} kB, B ${pb} kB (blob 32768 kB)"

grep -q '^BLOB A 33554432 bytes, 0 wrong' "$T/a.txt" && grep -q '^BLOB B 33554432 bytes, 0 wrong' "$T/b.txt" \
    && pass "both processes see every byte of the blob" || fail "blob contents"
[ "$pa" -ge 0 ] && [ "$pa" -lt 8192 ] && [ "$pb" -ge 0 ] && [ "$pb" -lt 8192 ] \
    && pass "the blob's pages are shared: each process's own memory there is under 8 MB" \
    || fail "private memory in the DLL: A ${pa} kB, B ${pb} kB"
grep -q '^WDATA A 85 2' "$T/a.txt" && grep -q '^WDATA B 1 2' "$T/b.txt" && grep -q '^WDATA_AFTER B 1' "$T/b.txt" \
    && pass "writable data stays each process's own (copy-on-write)" || fail "writable data: $(grep WDATA "$T/a.txt" "$T/b.txt" | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
