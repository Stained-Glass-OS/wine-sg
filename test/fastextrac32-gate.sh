#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# extrac32 (patch 3295): /A follows a cabinet chain, file name arguments (wildcards) choose what is extracted
# or listed. The test cabinets (a two-cabinet set where big.bin continues from set1.cab into set2.cab) are
# written here by python and checked with cabextract when it is installed.
#
#   WINE=/opt/wine-sg/bin/wine test/fastextrac32-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (programs/extrac32): SG_MUTANT_EXTRAC_* (see work/muts-extrac.sh).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastextrac32.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/cabs"
python3 - "$C/cabs" <<'EOP'
import struct, sys
out = sys.argv[1]
A = b'alpha-file\n'                       # 11 bytes
BIG = bytes((i * 7 + 3) & 255 for i in range(2000))
B = b'beta!\n'                            # 6 bytes
def cfile(size, off, ifolder, name):
    return struct.pack('<IIHHHH', size, off, ifolder, 0x5821, 0x6000, 0x20) + name + b'\0'
def cdata(item):
    data, uncomp = item
    return struct.pack('<IHH', 0, len(data), uncomp) + data
def cab(set_id, num, prev, nxt, files, folders):
    flags = (1 if prev else 0) | (2 if nxt else 0)
    names = b''
    if prev: names += prev + b'\0' + b'disk1\0'
    if nxt: names += nxt + b'\0' + b'disk2\0'
    hdr_len = 36 + len(names)
    files_b = b''.join(files)
    data_off = hdr_len + 8 * len(folders) + len(files_b)
    folder_b = b''
    data = b''
    for blocks in folders:
        folder_b += struct.pack('<IHH', data_off + len(data), len(blocks), 0)
        data += b''.join(cdata(b) for b in blocks)
    total = data_off + len(data)
    head = b'MSCF' + struct.pack('<IIIIIBBHHHHH', 0, total, 0, hdr_len + 8 * len(folders), 0, 3, 1, len(folders), len(files), flags, set_id, num)
    return head + names + folder_b + files_b + data
# big.bin is one data block cut in two between the cabinets: the first part carries cbUncomp 0
# (continued), the second part carries the size of the whole block. a.txt and b.txt have folders of their own.
set1 = cab(7, 0, None, b'set2.cab', [cfile(len(A), 0, 0, b'a.txt'), cfile(len(BIG), 0, 0xFFFE, b'big.bin')],
           [[(A, len(A))], [(BIG[:1000], 0)]])
set2 = cab(7, 1, b'set1.cab', None, [cfile(len(BIG), 0, 0xFFFD, b'big.bin'), cfile(len(B), 0, 1, b'sub\\b.txt')],
           [[(BIG[1000:], len(BIG))], [(B, len(B))]])
open(out + '/set1.cab', 'wb').write(set1)
open(out + '/set2.cab', 'wb').write(set2)
open(out + '/big.expected', 'wb').write(BIG)
EOP
if command -v cabextract >/dev/null; then
    (cd "$T" && cabextract -q -d ex "$C/cabs/set1.cab" >/dev/null 2>&1; cmp ex/big.bin "$C/cabs/big.expected" >/dev/null 2>&1 && echo "note: cabinet set verified by cabextract" || echo "note: cabextract could not verify the set")
fi
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$C"
x() { timeout -s KILL 120 "$WINE" extrac32 "\$@" >"$T/last.txt" 2>&1; }
{
echo "---all"; x /Y /A /E /L 'C:\\o1' 'C:\\cabs\\set1.cab'; echo "rc=\$?"
echo "---one"; x /Y /E /L 'C:\\o2' 'C:\\cabs\\set1.cab'; echo "rc=\$?"
echo "---names"; x /Y /A /L 'C:\\o3' 'C:\\cabs\\set1.cab' a.txt; echo "rc=\$?"
echo "---wild"; x /Y /A /L 'C:\\o4' 'C:\\cabs\\set1.cab' '*.txt'; echo "rc=\$?"
echo "---list"; x /A /D 'C:\\cabs\\set1.cab'; cat "$T/last.txt"
echo "---listone"; x /D 'C:\\cabs\\set1.cab'; cat "$T/last.txt"
echo "---listname"; x /A /D 'C:\\cabs\\set1.cab' 'big*'; cat "$T/last.txt"
echo "---second"; x /Y /E /L 'C:\\o5' 'C:\\cabs\\set2.cab'; echo "rc=\$?"
echo "---files"
for d in o1 o2 o3 o4 o5; do ( cd "$C/\$d" 2>/dev/null && find . -type f | sort | sed "s|^|\$d |" ); done
if cmp "$C/o1/big.bin" "$C/cabs/big.expected" >/dev/null 2>&1; then echo "bigok"; else echo "bigbad"; fi
if cmp "$C/o2/big.bin" "$C/cabs/big.expected" >/dev/null 2>&1; then echo "o2big complete"; fi
echo "---end"
} | tr -d '\r' > "$T/out.txt"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
fails=0
sec() { awk -v n="---$1" '$0==n{f=1;next} /^---/{f=0} f' "$T/out.txt"; }
check() { if sec "$2" | grep -aq -- "$3"; then echo "PASS  $1"; else echo "FAIL  $1"; fails=$((fails+1)); sec "$2" | sed 's/^/      /' | head -20; fi; }
nocheck() { if sec "$2" | grep -aq -- "$3"; then echo "FAIL  $1"; fails=$((fails+1)); sec "$2" | sed 's/^/      /' | head -20; else echo "PASS  $1"; fi; }
check "/A extracts the first cabinet's file" files '^o1 \./a.txt$'
check "/A follows the chain: the split file" files '^o1 \./big.bin$'
check "/A: the split file is intact" files '^bigok$'
check "/A: a file of the second cabinet (with its directory)" files '^o1 \./sub/b.txt$'
check "without /A the first cabinet's own file is extracted" files '^o2 \./a.txt$'
nocheck "without /A nothing from the second cabinet" files '^o2 .*b.txt'
nocheck "without /A the split file is not completed from the next cabinet" files '^o2big complete'
check "a file name argument selects a file" files '^o3 \./a.txt$'
nocheck "a file name argument excludes the others" files '^o3 .*\(big\|b\.txt\)'
check "wildcard selects a.txt" files '^o4 \./a.txt$'
check "wildcard reaches into the chain" files '^o4 \./sub/b.txt$'
nocheck "wildcard excludes big.bin" files '^o4 .*big.bin'
check "the second cabinet alone is extractable" files '^o5 \./sub/b.txt$'
check "/D lists a.txt" list 'a.txt'
check "/D /A lists the second cabinet's file" list 'b.txt'
check "/D /A lists the split file" list 'big.bin'
nocheck "/D without /A stops at the first cabinet" listone 'b.txt'
check "/D with a name filter" listname 'big.bin'
nocheck "/D name filter excludes others" listname 'a.txt'
if [ $fails = 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL"; cat "$T/out.txt"; exit 1
