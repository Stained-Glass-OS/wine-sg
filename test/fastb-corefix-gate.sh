#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Patches 3613-3614 (CORE: integrator must review): kernel32 console/file/mailslot units pass again:
# CON with TRUNCATE_EXISTING+read, a cancelled CopyFileEx deletes the partial destination, a zero-timeout mailslot read
# fails at once with STATUS_IO_TIMEOUT (kernel32 unit tests, from the build tree).
#   WINE=/path/to/build/wine test/fastb-corefix-gate.sh      SG_XVFB_N=261 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
EXE="$BUILD/dlls/kernel32/tests/x86_64-windows/kernel32_test.exe"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] && [ -f "$EXE" ] || { echo "SKIP: needs a build tree with the kernel32 tests"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-corefix.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
RC=0
for u in console file mailslot; do
  cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$(dirname "$EXE")"
timeout -s KILL 300 "$WINE" ./kernel32_test.exe $u 2>&1 </dev/null | tr -d '\r' > "$T/out-$u.txt"
EOS
  chmod +x "$T/run.sh"
  timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
  n=$(grep -c "Test failed\|succeeded inside todo" "$T/out-$u.txt")
  if [ "$n" = 0 ] && grep -q "$u: .* tests executed" "$T/out-$u.txt"; then echo "PASS  kernel32 $u"
  else echo "FAIL  kernel32 $u"; grep "Test failed\|succeeded inside" "$T/out-$u.txt" | head -5; RC=1; fi
done
exit $RC
