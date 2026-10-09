#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# propsys long tail (patches/sg/2000), on Xvfb: test/propkeyname-probe.c
# exercises PSGetPropertyKeyFromName (canonical-name table, generated from
# every PKEY_* in include/propkey.h), PSRegisterPropertySchema/
# PSUnregisterPropertySchema/PSRefreshPropertySchema (registry-backed
# bookkeeping instead of FIXME-and-succeed/E_NOTIMPL), PropertyStore_Commit
# (a real no-op on the in-memory store), and propvar.c's
# PROPVAR_ConvertNumber/PropVariantToBuffer/PropVariantToGUID/VariantToGUID/
# VariantToString extra VT_* coverage.
#
#   WINE=/opt/wine-sg/bin/wine test/propkeyname-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_PROPKEYFROMNAME, SG_MUTANT_SCHEMAREG,
# SG_MUTANT_CONVERTNUMBER, SG_MUTANT_PROPVARTOBUFFER, SG_MUTANT_VARIANTTOGUID,
# SG_MUTANT_VARIANTTOSTRING (dlls/propsys/propsys_main.c, propvar.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-propkeyname.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/propkeyname-probe.exe" "$HERE/propkeyname-probe.c" \
    -lpropsys -lole32 -loleaut32 -lshlwapi -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/propkeyname-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" propkeyname-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
