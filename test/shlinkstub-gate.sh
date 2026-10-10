#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32 shell link stub batch (patches/sg/2009): test/shlinkstub-probe.c
# exercises IShellLinkDataList (Add / Copy / RemoveDataBlock with the blocks
# kept across save and load, GetFlags / SetFlags with the user flags saved in
# the header), IPersistFile::SaveCompleted and IContextMenu::GetCommandString.
#
#   WINE=/opt/wine-sg/bin/wine test/shlinkstub-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/shell32/shelllink.c): SG_MUTANT_BLOCKS_DROPPED (a load drops
# unknown blocks), ADD_IGNORED, REMOVE_NOOP, FLAGS_DERIVED_ONLY (GetFlags
# without the set flags), VERB_EMPTY (GetCommandString gives no verb),
# FLAGS_NOSAVE (the header drops the set flags).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shlinkstub.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/shlinkstub-probe.exe" "$HERE/shlinkstub-probe.c" \
    -lole32 -loleaut32 -luuid -lshell32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/shlinkstub-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" shlinkstub-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  probe did not finish (crashed?)'
exit 1
