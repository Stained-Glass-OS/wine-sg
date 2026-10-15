#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mpr (patch 3565): universal names of a network drive, resource parents, extended error, NPSCopyString.
# A drive J: is made a network drive in a scratch prefix (a "unc/localhost/share" directory below dosdevices and the
# Wine\Drives type "network"), so the remote branch runs without any server.
#
#   WINE=/path/to/build/wine test/fastb-mpr-gate.sh      SG_XVFB_N=261 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-mpr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-mpr-probe.c" -lmpr || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$WINEPREFIX/dosdevices/unc/localhost/share"
ln -sfn unc/localhost/share "$WINEPREFIX/dosdevices/j:"
env DISPLAY= "$WINE" reg add 'HKLM\Software\Wine\Drives' /v 'j:' /d network /f >/dev/null 2>&1
"$WINESERVER" -k; sleep 1
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
T64="$BUILD/dlls/mpr/tests/x86_64-windows/mpr_test.exe"
T32="$BUILD/dlls/mpr/tests/i386-windows/mpr_test.exe"
for t in "\$T64" "\$T32"; do
  [ -f "\$t" ] || continue
  timeout -s KILL 120 "$WINE" "\$t" 2>&1 </dev/null | tr -d '\r' | grep -E "tests executed|Test failed" >> "$T/tests.out"
done
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
: > "$T/tests.out"
timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out" "$T/tests.out"
grep -q "^RESULT: PASS" "$T/probe.out" || exit 1
grep -q "SKIP-NO-REMOTE" "$T/probe.out" && { echo "FAIL  the network drive was not seen"; exit 1; }
grep -q "Test failed" "$T/tests.out" && exit 1
echo "RESULT: PASS"
exit 0
