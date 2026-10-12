#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32 batch (patches/sg/2048): test/shellord-probe.c drives IShellLink
# SetPath, SHGetStockIconInfo, ExtractAssociatedIcon, AddCommasW,
# ShortSizeFormatW, SHLocal*, SHIsBadInterfacePtr, DragQueryFileAorW, the She*
# helpers and PathProcessCommand.
#
#   WINE=/opt/wine-sg/bin/wine test/shellord-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/shell32): SG_MUTANT_SETPATH_RESOLVE (a program name is not
# looked up), SETPATH_DRIVE (no capital drive letter), STOCK_INVALID (unknown
# stock id leaves the fields), ASSOC_FALLBACK (the old generic icon),
# PPC_LONGEST (PPCF_LONGESTPOSSIBLE ignored), SHE_QUOTES (quotes kept),
# ADDCOMMAS (no grouping), BADIFACE (always usable).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shellord.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/shellord-probe.exe" "$HERE/shellord-probe.c" \
    -lole32 -loleaut32 -luuid -lshell32 -lshlwapi \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/shellord-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" shellord-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -n 212 -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
