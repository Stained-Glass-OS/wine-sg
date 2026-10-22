#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll (patch 5954): a 64-bit exe with DYNAMIC_BASE and HIGH_ENTROPY_VA but its relocations stripped
# (NSIS's 64-bit installers and uninstallers) is not ASLR-compatible: its heap, GlobalAlloc and
# VirtualAlloc stay below 2 GB as on Windows, so a script's 32-bit pointer arithmetic still works
# (foobar2000's uninstaller crashed reading a sign-extended heap pointer, qa5 2026-10-22). The same
# exe with relocations still gets high allocations (upstream's high-entropy behaviour).
# Mutant SG_MUTANT_RELOCHEAP (dlls/ntdll/unix/virtual.c).
#
#   WINE=/opt/wine-sg/bin/wine test/relocheap-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-relocheap.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wl,--dynamicbase,--high-entropy-va -o "$T/withrelocs.exe" "$HERE/relocheap-probe.c" \
    || { echo "FAIL  probe did not build"; exit 1; }
# the same exe as NSIS ships its stubs: IMAGE_FILE_RELOCS_STRIPPED set, the ASLR flags kept
python3 - "$T/withrelocs.exe" "$T/stripped.exe" <<'EOP'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read())
pe = struct.unpack_from('<I', d, 0x3c)[0]
ch = struct.unpack_from('<H', d, pe + 22)[0]
struct.pack_into('<H', d, pe + 22, ch | 0x0001)
dll = struct.unpack_from('<H', d, pe + 24 + 70)[0]
assert dll & 0x60 == 0x60, hex(dll)
open(sys.argv[2], 'wb').write(d)
EOP
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
probe() { timeout -s KILL 60 "$WINE" "$1" 2>/dev/null | tr -d '\r'; }
low() { python3 -c "import sys; v=[int(x.split('=')[1],16) for x in sys.argv[1].split()]; sys.exit(0 if v and all(0 < a < 0x80000000 for a in v) else 1)" "$1"; }
high() { python3 -c "import sys; v=[int(x.split('=')[1],16) for x in sys.argv[1].split()]; sys.exit(0 if v and all(a >= 0x100000000 for a in v) else 1)" "$1"; }
r=$(probe "$T/stripped.exe"); echo "# relocations stripped: $r"
if low "$r"; then echo "PASS  an exe without relocations keeps its heap, GlobalAlloc and VirtualAlloc below 2 GB"
else echo "FAIL  an exe without relocations got allocations above 2 GB: $r"; rc=1; fi
r=$(probe "$T/withrelocs.exe"); echo "# with relocations: $r"
if high "$r"; then echo "PASS  an exe with relocations and HIGH_ENTROPY_VA still allocates high"
else echo "FAIL  an exe with relocations and HIGH_ENTROPY_VA allocated low: $r"; rc=1; fi
exit $rc
