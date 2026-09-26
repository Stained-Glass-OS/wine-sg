#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Compression API in cabinet.dll (patches/sg/0036): MSZIP buffers as
# Windows writes them decode byte for byte, and ours round-trip.
#
# The Windows-written fixtures are test/fixtures/*.mszyml: compressed
# manifest indexes from winget's public community source (MIT-licensed
# winget-pkgs data). Their expected output is recomputed here with Python's
# zlib, independently of the code under test.
#
#   WINE=/opt/wine-sg/bin/wine test/compress-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-compress.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/compress-probe.exe" "$HERE/compress-probe.c" -lcabinet || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T/compress-probe.exe" "$HERE"/fixtures/*.mszyml "$C/"
run() { (cd "$C" && timeout -s KILL 120 "$WINE" compress-probe.exe "$@" 2>/dev/null | tr -d '\r'); }

for f in "$C"/*.mszyml; do
    b=$(basename "$f")
    run d "$b" "$b.out" >/dev/null
    if python3 - "$f" "$f.out" <<'PY'
import struct, sys, zlib
d = open(sys.argv[1], 'rb').read()
total, chunk = struct.unpack('<QQ', d[8:24])
pos, out = 24, b''
while len(out) < total:
    n = struct.unpack('<I', d[pos:pos + 4])[0]; c = d[pos + 4:pos + 4 + n]; pos += 4 + n; p = 0; part = b''
    while p < len(c):
        assert c[p:p + 2] == b'CK'
        o = zlib.decompressobj(-15, zdict=part[-32768:]) if part else zlib.decompressobj(-15)
        part += o.decompress(c[p + 2:]); p = len(c) - len(o.unused_data)
    out += part
sys.exit(0 if out == open(sys.argv[2], 'rb').read() else 1)
PY
    then pass "Windows-written $b decodes byte for byte"; else fail "$b decodes wrongly"; fi
done

for n in 0 1 32768 40000 200000 1500000; do
    out=$(run r "$n")
    [ "$out" = "roundtrip$n=ok" ] && pass "round trip, $n bytes" || fail "round trip, $n bytes: $out"
done

# A header with a damaged checksum byte is refused.
cp "$C/$(basename "$(ls "$C"/*.mszyml | head -1)")" "$C/bad.bin"
python3 -c "
import sys; p=sys.argv[1]; d=bytearray(open(p,'rb').read()); d[6]^=0xff; open(p,'wb').write(d)" "$C/bad.bin"
out=$(run d bad.bin bad.out)
case "$out" in *=605*|*query=605*) pass "a damaged header checksum is refused (ERROR_BAD_COMPRESSION_BUFFER)" ;;
    *) fail "a damaged header was accepted: $out" ;; esac

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
