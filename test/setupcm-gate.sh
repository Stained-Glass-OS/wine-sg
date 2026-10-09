#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# setupapi/cfgmgr32 stubs (patches/sg/2002), on Xvfb: test/setupcm-probe.c
# exercises machine handles, class enumeration, class registry properties,
# the selected device, destroying a driver list, ANSI device-node
# properties, custom device properties and INF [Version] queries. These
# were stubs that failed with ERROR_CALL_NOT_IMPLEMENTED or CR_FAILURE.
#
#   WINE=/opt/wine-sg/bin/wine test/setupcm-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/setupapi): SG_MUTANT_MACHINE, ENUMCLASSES, DEVPROPA,
# CLASSPROP, SELDEV, DRVDESTROY, CUSTOMPROP, INFVERSION.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-setupcm.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/setupcm-probe.exe" "$HERE/setupcm-probe.c" \
    -lsetupapi -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/setupcm-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" setupcm-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
