#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfplat helpers (patches/sg/2862): MFInitVideoFormat, MFGetUncompressedVideoFormat, MFConvertColorInfoFromDXVA,
# MFValidateMediaTypeSize, MFSerializeAttributesToStream / MFDeserializeAttributesFromStream, checked by
# test/mfrest-helpers-probe.c (values, error codes, round trip of every attribute type). Also fails when one of the
# formerly stubbed exports logs a FIXME ("stub" lines of the spec).
#
#   WINE=/opt/wine-sg/bin/wine test/mfrest-helpers-gate.sh     (WINESERVER=... when not beside $WINE)
# Mutants (mfplat, -DSG_MUTANT_x): MFIVF_RANGE, MFSER_UNKNOWN_DROP, MFVAL_ALWAYS_OK, MFDXVA_NO_MATRIX, MFUNC_ANY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
NAME=mfrest-helpers
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-$NAME.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/$NAME-probe.c" -lmf -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
cat "$T/probe.out"
if grep -E 'fixme:' "$T/stderr.log" | grep -E 'MFInitVideoFormat|MFGetUncompressedVideoFormat|MFConvertColorInfoFromDXVA|MFValidateMediaTypeSize|MFSerializeAttributesToStream|MFDeserializeAttributesFromStream|stub'; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
