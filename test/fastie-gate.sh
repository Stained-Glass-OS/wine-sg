#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ieframe batch (patches/sg/3013): test/fastie.vbs drives InternetExplorer.Application: geometry, Document, ReadyState, properties, bars, Quit.
#
#   WINE=/opt/wine-sg/bin/wine test/fastie-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
GECKO=$(ls -d /usr/share/wine/gecko/wine-gecko-*-x86_64 /opt/wine-sg/share/wine/gecko/wine-gecko-*-x86_64 2>/dev/null | head -1)
[ -n "$GECKO" ] || { echo "SKIP: needs wine-gecko"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastie.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
GVER=$(basename "$GECKO" | sed 's/wine-gecko-//; s/-x86_64//')
env DISPLAY= "$WINE" reg add "HKLM\\Software\\Wine\\MSHTML\\$GVER" /v GeckoPath /d "Z:$(echo "$GECKO" | tr / \\\\)" /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/fastie.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo fastie.vbs 2>&1 </dev/null | tr -d '\\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
