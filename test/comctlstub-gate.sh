#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# comctl32 stub batch (patches/sg/2007), on Xvfb: test/comctlstub-probe.c
# exercises CCM_SETWINDOWTHEME on 16 common controls (and ComboBoxEx's combo
# and edit), WM_PRINTCLIENT on the list view, tree view and month calendar, RB_SETPALETTE / RB_GETPALETTE (and the palette while painting)
# and the list view's LVSIL_GROUPHEADER image list.
#
#   WINE=/opt/wine-sg/bin/wine test/comctlstub-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/comctl32): SG_MUTANT_NOTHEME (listview.c), COMBOEX_CHILD
# (comboex.c), PALETTE_STORE and
# PALETTE_SELECT (rebar.c), GROUPIML (listview.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-comctlstub.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/comctlstub-probe.exe" "$HERE/comctlstub-probe.c" \
    -lcomctl32 -luxtheme -lgdi32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/comctlstub-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" comctlstub-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
