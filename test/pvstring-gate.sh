#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# propsys string conversion batch (patches/sg/2018): test/pvstring-probe.c
# converts numbers, times and vectors to text with PropVariantToString /
# ToStringAlloc / ToBSTR, checks truncation, PropVariantToBuffer of byte
# arrays, PSRefreshPropertySchema without COM and VariantToPropVariant of bad
# types.
#
#   WINE=/opt/wine-sg/bin/wine test/pvstring-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/propsys): SG_MUTANT_PV_NUMBERS (no text for numbers),
# PV_VECTOR_SEP, PV_DATE_FMT, PV_BUFFER_ARRAY (propvar.c), PV_COM_CHECK
# (propsys_main.c), PV_ILLEGAL (VariantToPropVariant).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pvstring.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/pvstring-probe.exe" "$HERE/pvstring-probe.c" \
    -lpropsys -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/pvstring-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" pvstring-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  probe did not finish (crashed?)'
exit 1
