#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32 batch (patches/sg/2049): test/shdataobj-probe.c drives the shell's
# data object (files and items that are not files), its format enumerator,
# SHGetItemFromDataObject, SHGetSetFolderCustomSettings and SHLimitInputEdit.
#
#   WINE=/opt/wine-sg/bin/wine test/shdataobj-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/shell32): SG_MUTANT_DATAOBJ_ALWAYS_FILES (every item has file
# formats), ENUM_SKIP (Skip leaves the position past the end), DOGIF_ERROR
# (a missing file list replaces the error), CUSTOM_READ (FCS_READ not done),
# LIMIT_CHAR (typed characters pass), LIMIT_PASTE (pasted text is not cleaned).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shdataobj.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/shdataobj-probe.exe" "$HERE/shdataobj-probe.c" \
    -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/shdataobj-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" shdataobj-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -n 212 -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
