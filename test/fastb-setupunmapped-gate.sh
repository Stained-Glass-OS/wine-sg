#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Patch 3610: SetupDiGetDeviceRegistryProperty on a property Windows knows but the device lacks fails with
# ERROR_INVALID_DATA (ntoskrnl test); unused and unknown numbers stay ERROR_INVALID_REG_PROPERTY.
#   WINE=/path/to/build/wine test/fastb-setupunmapped-gate.sh      SG_XVFB_N=261 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-setupunmapped.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-setupunmapped-probe.c" -lsetupapi -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
fail=0
check() { if grep -qx "$1" "$T/probe.out"; then echo "PASS  $1"; else echo "FAIL  $1"; fail=1; fi; }
check "devtypeA=13"
check "securityA=13"
check "removalA=13"
check "enumeratorA=13"
check "unused0A=3758096905"
check "unused2A=3758096905"
check "maxA=3758096905"
check "minus1A=3758096905"
check "devtypeW=13"
check "securityW=13"
check "removalW=13"
check "enumeratorW=13"
check "unused0W=3758096905"
check "unused2W=3758096905"
check "maxW=3758096905"
check "minus1W=3758096905"
exit $fail
