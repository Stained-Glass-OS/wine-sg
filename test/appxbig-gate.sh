#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package of many files is read in time (patches/sg/7718). The archive's entries and the block
# map's files were found by name with a linear search, once per file and for each check:
# Nexusbird Motion (32823 files, 194 MB) spent 682 s in AddPackageAsync before writing its first
# file (870 s in all; 17 s now), JASP (1.3 GB) more than fifteen minutes. A name twice in the
# block map (case-insensitively) is still refused.
#   WINE=/opt/wine-sg/bin/wine test/appxbig-gate.sh
# Mutant: SG_MUTANT_APPX_LINEAR_NAMES (dlls/appxpackaging: the linear lookups back).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
FILES="${FILES:-30000}"
LIMIT_MS="${LIMIT_MS:-30000}"
RC=0
for t in "$MINGW" python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-appxbig.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
trap '"$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/appxbig-probe.c" -lole32 -lshlwapi || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/"
python3 - "$HERE" "$C" "$FILES" <<'PY'
import sys, zipfile, argparse
sys.path.insert(0, sys.argv[1])
import mkmsix
out, n = sys.argv[2], int(sys.argv[3])
a = argparse.Namespace(name='StainedGlass.BigTest', version='1.0.0.0', arch='x64', publisher=mkmsix.PUBLISHER,
                       framework=False, display_name='Big', resource_id=None, language=[], dep=[], app=[],
                       allow_external=False, extensions=None, package_extensions=None)
files = [('d%03d\\f%05d.txt' % (i // 100, i), b'x%d' % i, zipfile.ZIP_STORED) for i in range(n)]
files.append(('AppxManifest.xml', mkmsix.manifest(a), zipfile.ZIP_DEFLATED))
mkmsix.write_zip(out + '/big.msix', files, mkmsix.CONTENT_TYPES)
dup = [('Assets\\dup.txt', b'a', zipfile.ZIP_STORED), ('assets\\DUP.txt', b'b', zipfile.ZIP_STORED),
       ('AppxManifest.xml', mkmsix.manifest(a), zipfile.ZIP_DEFLATED)]
mkmsix.write_zip(out + '/dup.msix', dup, mkmsix.CONTENT_TYPES)
PY
[ -f "$C/big.msix" ] || { echo "FAIL  the packages could not be made"; exit 1; }
out=$(cd "$C" && timeout -s KILL 900 "$WINE" 'C:\probe.exe' 'C:\big.msix' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
want=$FILES
printf '%s\n' "$out" | grep -qx "Reader=0x00000000" && echo "PASS  a package of $FILES files opens" || { echo "FAIL  the package did not open"; RC=1; }
printf '%s\n' "$out" | grep -qx "Files=$want" && printf '%s\n' "$out" | grep -qx "Found=$want" \
    && echo "PASS  every payload file, each found in the block map" || { echo "FAIL  files/found"; RC=1; }
ms=$(printf '%s\n' "$out" | sed -n 's/^Ms=//p')
if [ -n "$ms" ] && [ "$ms" -lt "$LIMIT_MS" ]; then echo "PASS  in $ms ms (under $LIMIT_MS)"
else echo "FAIL  it took ${ms:-forever} ms (limit $LIMIT_MS)"; RC=1; fi
out=$(cd "$C" && timeout -s KILL 300 "$WINE" 'C:\probe.exe' 'C:\dup.msix' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qx "Reader=0x80080205" && echo "PASS  a name twice in the block map (another case) is refused (APPX_E_INVALID_BLOCKMAP)" \
    || { echo "FAIL  the duplicate: $(printf '%s' "$out" | tr '\n' ' ')"; RC=1; }
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
