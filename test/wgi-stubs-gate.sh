#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.gaming.input's IInspectable and factory stubs (patches/sg/2800), on
# Xvfb: test/wgi-stubs-probe.c asks every factory and object the DLL hands out
# (no controller needed) for its GetIids, GetTrustLevel and GetRuntimeClassName,
# an unknown IID, ActivateInstance, the custom-factory registration calls and
# the empty collections the statics return. These were FIXME stubs returning
# E_NOTIMPL. A device-bound stub (battery, headset, version info, labels) needs
# a controller: see test/wgi-device-gate.sh.
#
#   WINE=/opt/wine-sg/bin/wine test/wgi-stubs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (windows.gaming.input, -DSG_MUTANT_x): WGI_IIDS (GetIids drops an
# interface), WGI_TRUST (PartialTrust), WGI_VECTOR_NAME (the view is named as a
# vector), WGI_REGISTER_NULL (a NULL factory is accepted).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgistubs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wgi-stubs-probe.exe" "$HERE/wgi-stubs-probe.c" -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgi-stubs-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wgi-stubs-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
