#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# comdlg32 item dialog batch (patches/sg/2006), on Xvfb: test/comdlgitem-probe.c
# exercises IOleWindow::ContextSensitiveHelp, ClearClientData, SetFilter and
# IncludeObject, AddPlace, SetNavigationRoot, the file type index, the
# IFileSaveDialog property / SetSaveAsItem methods, the ICommDlgBrowser3
# callbacks and IFileDialogCustomize item text / error results.
#
#   WINE=/opt/wine-sg/bin/wine test/comdlgitem-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/comdlg32/itemdlg.c): SG_MUTANT_CTXHELP, CLEARCLIENT, FILTER,
# PLACE, NAVROOT, TYPEINDEX, SAVEAS, PROPS, VIEWFLAGS, CURFILTER, ITEMTEXT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-comdlgitem.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/comdlgitem-probe.exe" "$HERE/comdlgitem-probe.c" \
    -lshell32 -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/comdlgitem-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" comdlgitem-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
