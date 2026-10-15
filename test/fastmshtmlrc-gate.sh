#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml recheck round (patch 3282): IDispatchEx::GetMemberProperties, pointer capture, createRangeCollection,
# XMLHttpRequest.send bodies (a local echo server), ParseString(PARSE_ABSOLUTIFYIE40URLS), navigator.userProfile,
# ITravelLogClient::LoadHistoryPosition, script onerror. 64- and 32-bit probes.
# Mutants: SG_MUTANT_RC_GMP, _PTRCAP, _RANGECOLL, _XHR, _PARSEABS, _OPSPROFILE, _LHP
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlrc-gate.sh
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
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastmshtmlrc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
SRV=
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/p64.exe" "$HERE/fastmshtmlrc-probe.c" -lole32 -loleaut32 -luuid -lshlwapi -lurlmon \
    || { echo "FAIL  64-bit probe did not build"; exit 1; }
have32=0
if command -v "$MINGW32" >/dev/null; then
    TMPDIR=/var/tmp "$MINGW32" -O1 -o "$T/p32.exe" "$HERE/fastmshtmlrc-probe.c" -lole32 -loleaut32 -luuid -lshlwapi -lurlmon && have32=1
fi
python3 "$HERE/fastmshtmlrc-echo.py" "$T/port" &
SRV=$!
n=0; while [ ! -s "$T/port" ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n+1)); done
PORT=$(cat "$T/port" 2>/dev/null)
[ -n "$PORT" ] || { echo "FAIL  echo server did not start"; exit 1; }
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
