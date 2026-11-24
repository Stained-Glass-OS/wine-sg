#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32: SHOpenPropSheetW (exported by name, ordinal 80) and SHOpenPropSheetA (ordinal 707) as Microsoft documents
# them (patches/sg/9922; test/propsheet-probe.c, test/propsheet-handler.c). Both were spec stubs and the W name was
# not exported, so a program importing it did not start. A test property sheet handler (two pages) is registered
# under a class key; the sheet gets its pages from the key's PropertySheetHandlers and the default CLSID, the
# handler gets the data object and the key, the start page is honoured, no pages is FALSE.
# Mutant: SG_MUTANT_PROPSHEET_NO_HANDLERS (dlls/shell32/shdialogs.c). Windows: SG_GATE_DISPLAY=:N, else an Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/propsheet-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-propsheet.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lshell32 -lole32 -luuid -ladvapi32"
{ TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/propsheet-probe.c" $LIBS &&
  TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/propsheet-probe.c" $LIBS &&
  TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -shared -o "$T/propsheet-handler.dll" "$HERE/propsheet-handler.c" -lole32 -luuid -lcomctl32 &&
  TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -shared -Wl,--kill-at -o "$T/propsheet-handler32.dll" "$HERE/propsheet-handler.c" -lole32 -luuid -lcomctl32
} 2>"$T/cc.log" || { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
mkdir -p "$WINEPREFIX"
sg_gate_display || exit $?
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/propsheet-handler.dll" "$T/propsheet-handler32.dll" "$WINEPREFIX/drive_c/"
rc=0
for p in p64 p32; do
    echo "== $p"
    (cd "$T" && timeout -s KILL 180 "$WINE" "$T/$p.exe" </dev/null 2>/dev/null) | tr -d '\r' > "$T/$p.out"
    cat "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
