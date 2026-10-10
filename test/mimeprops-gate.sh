#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# inetcomm batch (patches/sg/2046): test/mimeprops-probe.c drives an IMimeBody:
# SetPropInfo, AppendProp, CopyProps, MoveProps, DeleteExcept, QueryProp, Clone,
# EnumProps and its enumerator, the display name, IsType and CopyTo.
#
#   WINE=/opt/wine-sg/bin/wine test/mimeprops-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/inetcomm/mimeole.c): SG_MUTANT_PROPS_APPEND (AppendProp
# replaces), PROPS_REPLACE (a copy keeps the destination's old instances),
# ENUM_NAMES (EPF_NONAME ignored), ISTYPE_ATT (every body is an attachment),
# QUERY_CASE (QueryProp ignores case sensitivity).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mimeprops.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mimeprops-probe.exe" "$HERE/mimeprops-probe.c" \
    -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mimeprops-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mimeprops-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -n 212 -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
