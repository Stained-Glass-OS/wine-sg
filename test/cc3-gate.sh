#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# comctl32 batch (patches/sg/2050): test/cc3-probe.c drives ComboBoxEx
# CBEM_GETITEM with callback fields, HDM_SETORDERARRAY with bad entries and
# the edit control's WM_SIZE / WM_DESTROY results.
#
#   WINE=/opt/wine-sg/bin/wine test/cc3-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/comctl32): SG_MUTANT_COMBOEX_NOCALLBACK (GETITEM does not ask),
# HEADER_ORDER (entries are stored as given), EDIT_SIZE_RESULT (WM_SIZE 0).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cc3.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
printf '1 24 "%s"\n' "$HERE/theme-gallery.manifest" > "$T/m.rc"
"$WINDRES" "$T/m.rc" -O coff -o "$T/m.o" || { echo "FAIL  manifest did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/cc3-probe.exe" "$HERE/cc3-probe.c" "$T/m.o" \
    -lcomctl32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/cc3-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cc3-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -n 212 -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
