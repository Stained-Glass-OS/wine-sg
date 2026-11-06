#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shlwapi property bag helpers (patches/sg/2040), on Xvfb: test/propbag-probe.c
# makes a bag with SHCreatePropertyBagOnMemory and reads and writes typed
# values through SHPropertyBag_Read.../Write.../Delete (by ordinal).
#
#   WINE=/opt/wine-sg/bin/wine test/propbag-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/shlwapi/ordinal.c): SG_MUTANT_PROPBAG_READONLY (a read-only bag
# accepts writes), PROPBAG_POINT (points are stored under one name),
# PROPBAG_DELETE (Delete is a no-op).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-propbag.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/propbag-probe.exe" "$HERE/propbag-probe.c" \
    -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/propbag-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" propbag-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
