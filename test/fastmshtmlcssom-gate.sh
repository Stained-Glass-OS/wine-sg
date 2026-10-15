#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml MSH-CSSOM (DECISIONS-2): performance/timing/navigation toJSON objects (script Objects).
# Mutants: SG_MUTANT_CSSOM_JSON, _JSONNAV
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlcssom-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastmshtmlcssom.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
SRV=
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/p64.exe" "$HERE/fastmshtmlcssom-probe.c" -lole32 -loleaut32 -luuid -lshlwapi -lurlmon \
    || { echo "FAIL  64-bit probe did not build"; exit 1; }
have32=0
if command -v "$MINGW32" >/dev/null; then
    TMPDIR=/var/tmp "$MINGW32" -O1 -o "$T/p32.exe" "$HERE/fastmshtmlcssom-probe.c" -lole32 -loleaut32 -luuid -lshlwapi -lurlmon && have32=1
fi
PORT=
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/p*.exe "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" p64.exe $PORT 2>/dev/null </dev/null | tr -d '\r' | sed 's/^/64: /' > "$T/probe.out"
[ $have32 = 1 ] && timeout -s KILL 180 "$WINE" p32.exe $PORT 2>/dev/null </dev/null | tr -d '\r' | sed 's/^/32: /' >> "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 500 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
want=1; [ $have32 = 1 ] && want=2
[ "$(grep -c 'RESULT: PASS' "$T/probe.out")" = $want ] && ! grep -q FAIL "$T/probe.out" && exit 0
exit 1
