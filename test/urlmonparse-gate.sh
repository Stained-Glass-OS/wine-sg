#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# urlmon parsing and helpers (patches/sg/2003), on Xvfb:
# test/urlmonparse-probe.c exercises the CoInternetParseUrl actions that
# were E_NOTIMPL, URIs from other IUri implementations in the combine,
# parse, builder and IsEqual entry points, IsValidURL, RegisterMediaTypes,
# GetClassFileOrMime and CopyStgMedium for GDI-style media.
#
#   WINE=/opt/wine-sg/bin/wine test/urlmonparse-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/urlmon): SG_MUTANT_FOREIGNURI, PARSEURL, PARSEESCAPE,
# ISVALIDURL, REGMEDIA, CLASSMIME, COPYGDI.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-urlmonparse.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/urlmonparse-probe.exe" "$HERE/urlmonparse-probe.c" \
    -lurlmon -lole32 -loleaut32 -ladvapi32 -lgdi32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/urlmonparse-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" urlmonparse-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
