#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# setupapi batch (patches/sg/2054): test/diskspace2-probe.c drives the disk
# space list filled from inf sections (SetupAddSectionToDiskSpaceList,
# SetupAddInstallSectionToDiskSpaceList and their removals) and
# SetupAdjustDiskSpaceList, SetupGetSourceFileSize, SetupRemoveFromSourceList
# and SetupSetDirectoryIdEx.
#
#   WINE=/opt/wine-sg/bin/wine test/diskspace2-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/setupapi): SG_MUTANT_ADJUST_IGNORED (an amount changes
# nothing), SECTION_SIZE (every file is 0 bytes), SECTION_DELETE (a delete
# section is added as a copy), SOURCE_SIZE_ROUND (SetupGetSourceFileSize does
# not round), SOURCE_NO_REMOVE (SetupRemoveFromSourceList keeps the source).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-diskspace2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/diskspace2-probe.exe" "$HERE/diskspace2-probe.c" \
    -lsetupapi \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/diskspace2-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" diskspace2-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -n 212 -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
