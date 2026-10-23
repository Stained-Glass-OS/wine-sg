#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# appxpackaging: IAppxFactory::CreatePackageWriter (6056) -- it gave E_NOTIMPL. A package written
# with IAppxPackageWriter:
#  - takes a deflated payload file (several 64 KiB blocks), a stored one and one whose name has a
#    space (percent-encoded in the archive); refuses a repeated name, a footprint name, anything
#    after Close, and a manifest the manifest reader refuses;
#  - is a ZIP every reader takes: Python's zipfile (CRCs), the block map (each block's SHA-256 and
#    compressed size), [Content_Types].xml, and our own package reader (payload bytes, compression
#    options);
#  - is signed by osslsigncode (its AppX parser takes the layout).
#   WINE=/opt/wine-sg/bin/wine test/appxwriter-gate.sh   (mutant SG_MUTANT_WRITER_STORE_ALL)
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
T=$(mktemp -d /var/tmp/sg-appxwriter.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/appxwriter-probe.exe" "$HERE/appxwriter-probe.c" -lole32 -lshlwapi -luuid || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/appxwriter-probe.exe" "$C/"
python3 "$HERE/mkappx.py" "$C/good.msix" good
python3 - "$C" <<'PY'
import sys, zipfile
c = sys.argv[1]
z = zipfile.ZipFile(c + '/good.msix')
open(c + '/data.bin', 'wb').write(z.read('Public/data.bin'))
open(c + '/logo.png', 'wb').write(z.read('Assets/logo.png'))
open(c + '/manifest.xml', 'wb').write(z.read('AppxManifest.xml'))
open(c + '/bad.xml', 'wb').write(b'<NotAPackage/>')
PY

run() { (cd "$C" && timeout -s KILL 120 "$WINE" appxwriter-probe.exe "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got '$(printf '%s\n' "$1" | grep "^$2=" || echo none)'"; fi
}
out=$(run write 'C:\written.msix' 'C:\data.bin' 'C:\logo.png' 'C:\manifest.xml' 'C:\bad.xml')
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" CreatePackageWriter 0x00000000 "CreatePackageWriter makes a writer"
expect "$out" AddDeflated 0x00000000 "a deflated payload file is added"
expect "$out" AddStored 0x00000000 "a stored one"
expect "$out" AddSpaced 0x00000000 "and one with a space in its name"
expect "$out" AddDuplicate 0x80070057 "a repeated name (in another case) is refused"
expect "$out" AddFootprint 0x80070057 "and a footprint file's name"
expect "$out" Close 0x00000000 "Close writes the manifest, block map and content types"
expect "$out" AddAfterClose 0x8000ffff "nothing is added after Close"
expect "$out" CloseBadManifest 0x80080204 "a manifest the reader refuses is refused"

if python3 - "$C/written.msix" "$C/data.bin" <<'PY'
import sys, zipfile, hashlib, base64, re
z = zipfile.ZipFile(sys.argv[1])
assert z.testzip() is None, 'a CRC'
names = z.namelist()
assert names == ['Public/data.bin', 'Assets/logo.png', 'Docs/read%20me.txt', 'AppxManifest.xml', 'AppxBlockMap.xml',
                 '[Content_Types].xml'], names
assert z.read('Public/data.bin') == open(sys.argv[2], 'rb').read()
assert z.getinfo('Public/data.bin').compress_type == 8 and z.getinfo('Assets/logo.png').compress_type == 0
bm = z.read('AppxBlockMap.xml').decode()
files = re.findall(r'<File Name="([^"]+)" Size="(\d+)" LfhSize="(\d+)"[^>]*>(.*?)</File>', bm)
assert [f[0] for f in files] == ['Public\\data.bin', 'Assets\\logo.png', 'Docs\\read me.txt', 'AppxManifest.xml'], files
for name, size, lfh, blocks in files:
    data = z.read(name.replace('\\', '/').replace(' ', '%20'))
    hashes = re.findall(r'Hash="([^"]+)"', blocks)
    assert len(hashes) == (len(data) + 65535) // 65536, name
    for i, h in enumerate(hashes):
        assert base64.b64decode(h) == hashlib.sha256(data[i * 65536:(i + 1) * 65536]).digest(), (name, i)
    info = z.getinfo(name.replace('\\', '/').replace(' ', '%20'))
    assert int(lfh) == 30 + len(info.filename.encode()) + len(info.extra), (name, lfh)
    sizes = [int(s) for s in re.findall(r' Size="(\d+)"', blocks)]
    if info.compress_type == 8:
        assert sum(sizes) == info.compress_size, (name, sum(sizes), info.compress_size)
ct = z.read('[Content_Types].xml').decode()
for frag in ('Extension="bin" ContentType="application/octet-stream"', 'Extension="png" ContentType="image/png"',
             'Extension="txt" ContentType="text/plain"', 'application/vnd.ms-appx.blockmap+xml'):
    assert frag in ct, frag
PY
then pass "the archive, its block map (hashes, compressed block sizes, header sizes) and content types are right"
else fail "the written package does not check out"; fi

out=$(run read 'C:\written.msix' 'C:\data.bin')
expect "$out" CreatePackageReader 0x00000000 "our package reader opens it (and checks its block map)"
expect "$out" PayloadCount 3 "its three payload files"
expect "$out" Data "1 204800 1" "the deflated file reads back byte for byte"
expect "$out" Spaced 0x00000000 "the name with a space is found"
expect "$out" Stored "0x00000000 0" "the stored one is stored"

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/k.pem" -out "$T/c.pem" -days 2 \
    -subj "/CN=Stained Glass Test Publisher" -addext "extendedKeyUsage=codeSigning" 2>/dev/null
if osslsigncode sign -certs "$T/c.pem" -key "$T/k.pem" -h sha256 -in "$C/written.msix" -out "$T/signed.msix" >/dev/null 2>&1
then pass "osslsigncode signs the written package"; else fail "osslsigncode does not take the written package"; fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
