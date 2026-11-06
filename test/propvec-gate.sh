#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# propsys vectors and elements (patches/sg/2038), on Xvfb: test/propvec-probe.c
# drives PropVariantGetElementCount, the typed Get...Elem accessors,
# InitPropVariantFrom...Vector, PropVariantTo...Vector(Alloc), the WithDefault
# conversions and ClearPropVariantArray.
#
#   WINE=/opt/wine-sg/bin/wine test/propvec-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/propsys/propvar.c): SG_MUTANT_PROPVEC_COUNT (a scalar counts
# 0), PROPVEC_BOOL (boolean vectors keep their C values), DOUBLE_FRACTION (a
# double loses its fraction), PROPVEC_SMALL (a
# too small buffer is filled anyway).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-propvec.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/propvec-probe.exe" "$HERE/propvec-probe.c" \
    -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/propvec-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" propvec-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
