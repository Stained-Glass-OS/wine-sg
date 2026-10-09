#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# OLE32 breadth (patches/sg/1673), on Xvfb: test/ole32misc-probe.c
# round-trips an object through an objref moniker (Save/Load, display
# name and MkParseDisplayName, BindToObject), gets class and file icons
# (OleGetIconOfClass, OleGetIconOfFile), enumerates registered data
# formats (OleRegEnumFormatEtc), asks OleQueryLinkFromData, starts OLE
# by OleInitializeWOW, and skips and clones the property enumerators.
# These were FIXMEs and stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/ole32misc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_OBJREF_SAVE (ole32/pointermoniker.c), SG_MUTANT_NO_REG_FORMATS (ole32/ole2stubs.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ole32misc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/ole32misc-probe.exe" "$HERE/ole32misc-probe.c" -lole32 -loleaut32 -luuid -lgdi32 -ladvapi32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/ole32misc-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" ole32misc-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
