#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# appxpackaging: IAppxBundleFactory::CreateBundleWriter and IAppxPackageWriter2/3 (6060) -- the bundle
# writer gave E_NOTIMPL and the package writer had no Writer2/3.
#  - IAppxPackageWriter3::AddPayloadFiles writes a batch, refusing a batch with a repeated name whole;
#    IAppxPackageWriter2::Close writes a content group map (refusing one that is not), which the package
#    reader finds as the CONTENTGROUPMAP footprint file;
#  - a bundle of an x64 and an x86 package and a resource package (made by our package writer): packages
#    stored, the bundle manifest's identity from them and its version as given, each package's type,
#    architecture, resource ID, languages, offset (where its bytes are) and size, the block map's SHA-256
#    hashes, content types; a package of another app, a repeated name, a name that is not a package's and
#    a file that is not a package are refused, and our bundle reader opens the result;
#  - IAppxBundleWriter3: a referenced package (a flat bundle's) is listed without an offset and not
#    stored; Close takes the hash method (SHA-512 here, an unknown one refused);
#  - no application package: no bundle; version 0: one made from the time.
#   WINE=/opt/wine-sg/bin/wine test/appxbundlew-gate.sh   (mutants SG_MUTANT_BUNDLE_ALL_APPLICATION,
#   SG_MUTANT_BUNDLE_MIXED_APPS, SG_MUTANT_BUNDLE_SHA256_ONLY, SG_MUTANT_WRITER2_ACCEPT_ANY_MAP,
#   SG_MUTANT_WRITER3_NO_PRECHECK)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" python3; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-appxbundlew.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/appxbundlew-probe.exe" "$HERE/appxbundlew-probe.c" -lole32 -lshlwapi -luuid || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/appxbundlew-probe.exe" "$C/"
python3 - "$C" "$HERE" <<'PY'
import sys
c, here = sys.argv[1], sys.argv[2]
sys.path.insert(0, here)
import mkappx
m = mkappx.MANIFEST.decode()
open(c + '/manifest-x64.xml', 'w').write(m)
open(c + '/manifest-x86.xml', 'w').write(m.replace('ProcessorArchitecture="x64"', 'ProcessorArchitecture="x86"'))
open(c + '/manifest-other.xml', 'w').write(m.replace('StainedGlass.AppxTest', 'StainedGlass.Other'))
fr = m.replace('ProcessorArchitecture="x64"', 'ProcessorArchitecture="neutral" ResourceId="split.language-fr"')
fr = fr.replace('<Logo>', '<ResourcePackage>true</ResourcePackage><Logo>')
fr = fr.replace('<Dependencies>', '<Resources><Resource Language="fr-fr" /></Resources><Dependencies>')
start, end = fr.index('<Applications>'), fr.index('</Applications>') + len('</Applications>')
open(c + '/manifest-fr.xml', 'w').write(fr[:start] + fr[end:])
open(c + '/data.bin', 'wb').write(mkappx.payload())
open(c + '/cgm.xml', 'w').write('<?xml version="1.0" encoding="utf-8"?><ContentGroupMap '
    'xmlns="http://schemas.microsoft.com/appx/2016/sourcecontentgroupmap"><Required><ContentGroup Name="Required">'
    '<File Name="a.bin"/></ContentGroup></Required></ContentGroupMap>')
open(c + '/badcgm.xml', 'w').write('<NotAMap/>')
PY

out=$(cd "$C" && timeout -s KILL 180 "$WINE" appxbundlew-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect MakeX64 0x00000000 "our package writer makes the x64 package"
expect MakeX86 0x00000000 "the x86 one"
expect MakeFr 0x00000000 "a resource package"
expect MakeOther 0x00000000 "and another app's package"
expect QueryWriter2 0x00000000 "a package writer is an IAppxPackageWriter2"
expect QueryWriter3 0x00000000 "and an IAppxPackageWriter3"
expect QueryPackageAsBundle 0x80004002 "but not a bundle writer"
expect AddPayloadFilesRepeated 0x80070057 "AddPayloadFiles refuses a batch with a repeated name"
expect AddPayloadFiles 0x00000000 "and writes a good one"
expect CloseBadMap 0x8008020b "IAppxPackageWriter2::Close refuses what is not a content group map"
expect CloseWithMap 0x00000000 "and writes a content group map"
expect ReadW23 "0x00000000 2 1" "the package reader opens it: the two files (none of the refused batch) and the map"
expect QueryBundle2 0x00000000 "a bundle writer is an IAppxBundleWriter2"
expect QueryBundle3 0x00000000 "an IAppxBundleWriter3"
expect QueryBundle4 0x00000000 "an IAppxBundleWriter4"
expect QueryBundleAsPackage 0x80004002 "but not a package writer"
expect AddX64 0x00000000 "AddPayloadPackage takes the x64 package"
expect AddX86 0x00000000 "IAppxBundleWriter4::AddPayloadPackage the x86 one"
expect AddFr 0x00000000 "and the resource package"
expect AddOtherApp 0x80080204 "another app's package is refused"
expect AddRepeated 0x80070057 "a repeated name (in another case) is refused"
expect AddNotPackageName 0x80070057 "a name that is not a package's is refused"
expect AddNotPackage 0x80080206 "a file that is not a package is refused (APPX_E_CORRUPT_CONTENT)"
expect AddExternal 0x80004001 "external package references are not written (E_NOTIMPL)"
expect BundleClose 0x00000000 "Close writes the bundle"
expect AddAfterClose 0x8000ffff "nothing is added after Close"
expect ReadBundle "0x00000000 1.2.3.4; app_x64.msix 0 1 1; app_x86.msix 0 1 1; res_fr.msix 1 1 1" \
    "our bundle reader opens it: the version given, two application packages and a resource package"
expect FlatAddX64 0x00000000 "a flat bundle takes an embedded package"
expect FlatReference 0x00000000 "and a reference"
expect FlatBadHash 0x80070057 "an unknown hash method is refused"
expect FlatClose 0x00000000 "Close with SHA-512"
expect ReadFlat "0x00000000 2.0.0.0; app_x64.msix 0 1 1; app_x86.msix 0 0 1" \
    "our bundle reader opens the flat bundle; the reference has no offset"
expect CloseResourceOnly 0x80080204 "a bundle of only a resource package is refused"
expect CloseVersionZero 0x00000000 "a bundle of version 0"
expect done 1 "the probe ran to the end"

if python3 - "$C" <<'PY'
import sys, zipfile, hashlib, base64, re, time
c = sys.argv[1]

def check_blockmap(z, files, method, size):
    bm = z.read('AppxBlockMap.xml').decode()
    assert 'HashMethod="%s"' % method in bm, bm[:300]
    listed = re.findall(r'<File Name="([^"]+)" Size="(\d+)" LfhSize="(\d+)"[^>]*>(.*?)</File>', bm)
    assert [f[0] for f in listed] == files, listed
    for name, fsize, lfh, blocks in listed:
        data = z.read(name.replace('\\', '/'))
        hashes = re.findall(r'Hash="([^"]+)"', blocks)
        assert len(hashes) == (len(data) + 65535) // 65536, name
        for i, h in enumerate(hashes):
            assert base64.b64decode(h) == hashlib.new(size, data[i * 65536:(i + 1) * 65536]).digest(), (name, i)

raw = open(c + '/b.msixbundle', 'rb').read()
z = zipfile.ZipFile(c + '/b.msixbundle')
assert z.testzip() is None
names = z.namelist()
assert names == ['app_x64.msix', 'app_x86.msix', 'res_fr.msix', 'AppxMetadata/AppxBundleManifest.xml',
                 'AppxBlockMap.xml', '[Content_Types].xml'], names
man = z.read('AppxMetadata/AppxBundleManifest.xml').decode()
assert '<Identity Name="StainedGlass.AppxTest" Publisher="CN=Stained Glass Test Publisher" Version="1.2.3.4"/>' in man, man
pk = re.findall(r'<Package ([^>]*)>(.*?)</Package>', man)
assert len(pk) == 3, man
for (attrs, inner), name in zip(pk, ['app_x64.msix', 'app_x86.msix', 'res_fr.msix']):
    a = dict(re.findall(r'(\w+)="([^"]*)"', attrs))
    info = z.getinfo(name)
    assert info.compress_type == 0, name
    data = open(c + '/' + name, 'rb').read()
    assert a['FileName'] == name and int(a['Size']) == len(data), a
    off = int(a['Offset'])
    assert raw[off:off + len(data)] == data, (name, off)
    assert a['Version'] == '1.2.3.4', a
    if name == 'res_fr.msix':
        assert a['Type'] == 'resource' and a['ResourceId'] == 'split.language-fr' and a['Architecture'] == 'neutral', a
        assert '<Resource Language="fr-fr"/>' in inner, inner
    else:
        assert a['Type'] == 'application' and 'ResourceId' not in a, a
        assert a['Architecture'] == name[4:7], a
check_blockmap(z, ['app_x64.msix', 'app_x86.msix', 'res_fr.msix', 'AppxMetadata\\AppxBundleManifest.xml'],
               'http://www.w3.org/2001/04/xmlenc#sha256', 'sha256')
ct = z.read('[Content_Types].xml').decode()
for frag in ('Extension="msix" ContentType="application/vnd.ms-appx"', 'application/vnd.ms-appx.bundlemanifest+xml',
             'application/vnd.ms-appx.blockmap+xml'):
    assert frag in ct, (frag, ct)

z = zipfile.ZipFile(c + '/flat.msixbundle')
assert z.namelist() == ['app_x64.msix', 'AppxMetadata/AppxBundleManifest.xml', 'AppxBlockMap.xml', '[Content_Types].xml'], z.namelist()
man = z.read('AppxMetadata/AppxBundleManifest.xml').decode()
ref = re.search(r'<Package ([^>]*FileName="app_x86.msix"[^>]*)>', man).group(1)
assert 'Offset=' not in ref and 'Size="%d"' % len(open(c + '/app_x86.msix', 'rb').read()) in ref, ref
check_blockmap(z, ['app_x64.msix', 'AppxMetadata\\AppxBundleManifest.xml'], 'http://www.w3.org/2001/04/xmlenc#sha512', 'sha512')

z = zipfile.ZipFile(c + '/w23.msix')
assert z.namelist() == ['a.bin', 'b.bin', 'AppxManifest.xml', 'AppxMetadata/ContentGroupMap.xml', 'AppxBlockMap.xml',
                        '[Content_Types].xml'], z.namelist()
assert z.read('AppxMetadata/ContentGroupMap.xml') == open(c + '/cgm.xml', 'rb').read()

man = zipfile.ZipFile(c + '/now.msixbundle').read('AppxMetadata/AppxBundleManifest.xml').decode()
v = re.search(r'<Identity [^>]*Version="(\d+)\.(\d+)\.(\d+)\.(\d+)"', man).groups()
now = time.gmtime()
assert int(v[0]) == now.tm_year and int(v[1]) // 100 in (now.tm_mon, now.tm_mon - 1) and v[3] == '0', v
PY
then pass "the archives: stored packages at their offsets, manifests, SHA-256/SHA-512 block maps, content types, the map, the time version"
else fail "the written bundles do not check out"; fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
