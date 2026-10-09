#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Compressed (zipped) folders (patches/sg/1658), 64- and 32-bit cscript:
# test/zipfolder.vbs opens a Python-made archive (deflated, stored, ZIP64,
# a UTF-8 name, nested folders, an entry climbing out with "..") through
# Shell.Application -- NameSpace, Items, ParseName, details, a path into its
# folders -- extracts it with CopyHere, makes a new archive from the empty
# one scripts write and adds files and a folder to it, renames and deletes
# inside it. Python then checks the extracted bytes and that our archive
# passes testzip with the contents expected. A .zip was not a folder at all.
#
#   WINE=/opt/wine-sg/bin/wine test/zipfolder-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_JUNCTION (shell32/shlfolder.c), SG_MUTANT_NO_ZIP_ADD,
# SG_MUTANT_NO_ZIP64 (shell32/zipfolder.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-zipfolder.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
cp "$HERE/zipfolder.vbs" "$T/"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for arch in 64 32; do
    W="$T/w$arch"
    rm -rf "$W"; mkdir -p "$W/out" "$W/one" "$W/z64"
    python3 - "$W" <<'EOF'
import sys, zipfile
d = sys.argv[1]
with zipfile.ZipFile(d + '/src.zip', 'w') as z:
    z.writestr('readme.txt', 'hello zip\r\n' * 500, compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('stored.bin', bytes(range(256)) * 4, compress_type=zipfile.ZIP_STORED)
    z.writestr('docs/', '')
    z.writestr('docs/inner/deep.txt', 'deep content ' * 100, compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('caf\u00e9.txt', 'unicode name', compress_type=zipfile.ZIP_DEFLATED)
    with z.open('big64.txt', 'w', force_zip64=True) as f:
        f.write(b'zip64 sized ' * 1000)
    z.writestr(zipfile.ZipInfo('../evil.txt'), 'evil')
# an archive whose central directory is ZIP64 throughout (sizes, offset and
# the end records), as tools write for large ones
import struct, zlib
data = b'zip64 entry ' * 64
name = b'z64.txt'
crc = zlib.crc32(data)
local = struct.pack('<IHHHHHIIIHH', 0x04034b50, 45, 0, 0, 0, 0x21, crc, 0xffffffff, 0xffffffff, len(name), 20) + name + \
        struct.pack('<HHQQ', 1, 16, len(data), len(data)) + data
z64 = struct.pack('<HHQQQ', 1, 24, len(data), len(data), 0)
central = struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 45, 45, 0, 0, 0, 0x21, crc, 0xffffffff, 0xffffffff,
                      len(name), len(z64), 0, 0, 0, 0, 0xffffffff) + name + z64
cd_offset = len(local)
eocd64 = struct.pack('<IQHHIIQQQQ', 0x06064b50, 44, 45, 45, 0, 0, 1, 1, len(central), cd_offset)
loc = struct.pack('<IIQI', 0x07064b50, 0, cd_offset + len(central), 1)
eocd = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 0xffff, 0xffff, 0xffffffff, 0xffffffff, 0)
open(d + '/z64.zip', 'wb').write(local + central + eocd64 + loc + eocd)
EOF
    if [ $arch = 32 ]; then cs='C:\windows\syswow64\cscript.exe'; else cs=cscript; fi
    echo "== $arch-bit"
    ZW='Z:'"$(printf '%s' "$W" | tr / '\\')"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$cs" //nologo "Z:$(printf '%s' "$T/zipfolder.vbs" | tr / '\\')" "$ZW" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    python3 - "$W" <<'EOF' || RC=1
import sys, os, zipfile
d = sys.argv[1]
ok = True
def check(c, what):
    global ok
    print(('      PASS  ' if c else '      FAIL  ') + what)
    ok = ok and c
src = zipfile.ZipFile(d + '/src.zip')
p = os.path.join(d, 'z64', 'z64.txt')
check(os.path.exists(p) and open(p, 'rb').read() == b'zip64 entry ' * 64, 'an entry of a ZIP64 archive extracted')
for name in ['readme.txt', 'stored.bin', 'docs/inner/deep.txt', 'caf\u00e9.txt', 'big64.txt']:
    p = os.path.join(d, 'out', *name.split('/'))
    check(os.path.exists(p) and open(p, 'rb').read() == src.read(name), 'extracted %s byte for byte' % name)
try:
    z = zipfile.ZipFile(d + '/new.zip')
    check(z.testzip() is None, 'our archive passes testzip')
    names = sorted(z.namelist())
    print('      ' + ' '.join(names))
    check('docs/stored.bin' in names and 'docs/inner/deep.txt' in names and 'renamed.txt' not in names and 'readme.txt' not in names,
          'our archive holds what was added, renamed and deleted')
    check(z.read('docs/stored.bin') == src.read('stored.bin') and z.read('docs/inner/deep.txt') == src.read('docs/inner/deep.txt'),
          'our archive\'s contents')
except Exception as e:
    check(False, 'our archive opens: %s' % e)
sys.exit(0 if ok else 1)
EOF
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
