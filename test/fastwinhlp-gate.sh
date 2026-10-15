#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winhlp32: starts (shows its open dialog) without crashing; bookmark macros are covered by review only
#
#   WINE=/opt/wine-sg/bin/wine test/fastwinhlp-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastwinhlp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 8 "$WINE" winhlp32.exe >/dev/null 2>&1 </dev/null
echo \$? > "$T/rc"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 120 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
rc=$(cat "$T/rc" 2>/dev/null)
# 137 = still running when the timeout killed it: the open-file dialog is up
if [ "$rc" = 137 ]; then echo "RESULT: PASS"; exit 0; fi
echo "FAIL winhlp32 exited with $rc"; echo "RESULT: FAIL"; exit 1
