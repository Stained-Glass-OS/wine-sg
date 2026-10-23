#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# appxpackaging: block map readers and device capabilities (6055) -- GetBlockMap,
# CreateBlockMapReader, CreateValidatedBlockMapReader and GetDeviceCapabilities gave E_NOTIMPL.
#  - a package reader's block map: its files, a file's name, size and local file header size, its
#    blocks (hash size, compressed size), the hash method URI, the block map stream; an unknown name
#    is not found; ValidateFileHash takes the file's bytes and refuses a changed byte;
#  - the same from the block map XML on its own (CreateBlockMapReader);
#  - CreateValidatedBlockMapReader takes the block map with the package's signature (osslsigncode,
#    a certificate made for this run) and refuses a changed block map;
#  - the manifest's device capabilities, in order.
#   WINE=/opt/wine-sg/bin/wine test/blockmap-gate.sh   (mutants SG_MUTANT_BLOCKMAP_NO_VALIDATE,
#   SG_MUTANT_BLOCKMAP_SIGNATURE_IGNORED, SG_MUTANT_APPX_NO_DEVCAPS)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-blockmap.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/blockmap-probe.exe" "$HERE/blockmap-probe.c" -lole32 -lshlwapi -luuid || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/blockmap-probe.exe" "$C/"

python3 "$HERE/mkappx.py" "$C/good.msix" good
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.pem" -days 2 \
    -subj "/CN=Stained Glass Test Publisher" -addext "extendedKeyUsage=codeSigning" 2>/dev/null
osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$C/good.msix" -out "$C/signed.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the fixture"; exit 1; }
python3 - "$C" <<'PY'
import sys, zipfile
c = sys.argv[1]
z = zipfile.ZipFile(c + '/good.msix')
data = z.read('Public/data.bin')
open(c + '/data.bin', 'wb').write(data)
open(c + '/tampered.bin', 'wb').write(data[:1000] + bytes([data[1000] ^ 1]) + data[1001:])
s = zipfile.ZipFile(c + '/signed.msix')
bm = s.read('AppxBlockMap.xml')
open(c + '/blockmap.xml', 'wb').write(bm)
open(c + '/signature.p7x', 'wb').write(s.read('AppxSignature.p7x'))
open(c + '/blockmap-changed.xml', 'wb').write(bm.replace(b'LfhSize="', b'LfhSize="1', 1))
PY

run() { (cd "$C" && timeout -s KILL 120 "$WINE" blockmap-probe.exe "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got '$(printf '%s\n' "$1" | grep "^$2=" || echo none)'"; fi
}
LFH=$((30 + 15))
HASH=$(python3 -c "
import hashlib,zipfile,sys
d=zipfile.ZipFile(sys.argv[1]).read('Public/data.bin')[:65536]
print(hashlib.sha256(d).hexdigest()[:4])" "$C/good.msix")
BMSIZE=$(stat -c %s "$C/blockmap.xml")

out=$(run package 'C:\good.msix' 'C:\data.bin' 'C:\tampered.bin')
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" GetBlockMap 0x00000000 "a package reader gives its block map"
expect "$out" FileCount 3 "the block map's files"
expect "$out" File "Public\\data.bin 204800 $LFH" "a file's name, size and local file header size"
expect "$out" BlockCount 4 "its 64 KiB blocks"
case "$(printf '%s\n' "$out" | grep '^Block=')" in "Block=32 $HASH "*) pass "a block's SHA-256 hash" ;; *) fail "block: $(printf '%s\n' "$out" | grep '^Block=') (hash $HASH)" ;; esac
expect "$out" GetMissing 0x80070002 "an unknown file is not found"
expect "$out" HashMethod "0x00000000 http://www.w3.org/2001/04/xmlenc#sha256" "the hash method, as a URI"
expect "$out" Validate 1 "ValidateFileHash takes the file's bytes"
expect "$out" ValidateTampered 0 "and refuses a changed byte"
expect "$out" DeviceCapabilities "webcam,microphone," "the manifest's device capabilities, in order"

out=$(run standalone 'C:\blockmap.xml')
expect "$out" CreateBlockMapReader 0x00000000 "CreateBlockMapReader reads a block map on its own"
expect "$out" File "Public\\data.bin 204800 $LFH" "with the same files"
expect "$out" StreamSize "$BMSIZE" "GetStream gives the block map back"

out=$(run validated 'C:\blockmap.xml' 'C:\signature.p7x')
expect "$out" CreateValidatedBlockMapReader 0x00000000 "CreateValidatedBlockMapReader takes the signed block map"
out=$(run validated 'C:\blockmap-changed.xml' 'C:\signature.p7x')
case "$(printf '%s\n' "$out" | grep '^CreateValidatedBlockMapReader=')" in
    *=0x00000000|"") fail "a changed block map is not refused: $out" ;;
    *) pass "a changed block map is refused ($(printf '%s\n' "$out" | grep '^CreateValidatedBlockMapReader=' | cut -d= -f2))" ;;
esac
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
