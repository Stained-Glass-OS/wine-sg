#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cabinet XPRESS and FDITruncateCabinet (patch 3306): test/fastcab-probe.c
#
#   WINE=/opt/wine-sg/bin/wine test/fastcab-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastcab.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for arch in x86_64 i686; do
  for probe in fastxpress fastcabtrunc; do
    CC="$arch-w64-mingw32-gcc"
    command -v "$CC" >/dev/null || { echo "SKIP $arch: $CC missing"; continue; }
    TMPDIR=/var/tmp "$CC" -O1 -o "$T/$probe-$arch.exe" "$HERE/$probe-probe.c" -lcabinet \
        || { echo "FAIL  probe did not build ($probe $arch)"; exit 1; }
    cp "$T/$probe-$arch.exe" "$WINEPREFIX/drive_c/"
    cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" $probe-$arch.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
    chmod +x "$T/run.sh"
    timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
    echo "== $probe $arch"
    cat "$T/probe.out"
    grep -q '^RESULT: PASS' "$T/probe.out" || rc=1
  done
done
exit $rc
