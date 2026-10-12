#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# propsys FILETIME / STRRET / resource / serialization conversions
# (patches/sg/2045): test/propconv-probe.c drives InitPropVariantFromFileTime,
# PropVariantToFileTime(+Vector), VariantToFileTime/DosDateTime/Buffer, the
# StrRet and resource initialisers, InitPropVariantFromStringAsVector,
# vector element copies, ClearVariantArray, Stg(De)SerializePropVariant and
# PSGetNameFromPropertyKey.
#
#   WINE=/opt/wine-sg/bin/wine test/propconv-gate.sh
# Mutants (dlls/propsys): SG_MUTANT_FT_LOCAL (PSTF_LOCAL ignored),
# SG_MUTANT_STG_PAD (serialized values are not padded), SG_MUTANT_PSNAME
# (PSGetNameFromPropertyKey matches the pid only), SG_MUTANT_STRVEC (the
# string vector keeps empty items).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-propconv.XXXXXX)
export TZ=Asia/Tokyo WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
"$WINDRES" -O coff -o "$T/res.o" "$HERE/propconv-probe.rc" || { echo "FAIL  resource did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/propconv-probe.exe" "$HERE/propconv-probe.c" "$T/res.o" \
    -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/propconv-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" propconv-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -n 212 -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
