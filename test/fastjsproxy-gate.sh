#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jsproxy: the global object and script site (cut out of main.c) and the DLL on local PAC scripts
#
#   WINE=/opt/wine-sg/bin/wine test/fastjsproxy-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastjsproxy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
# the global object and the site are taken from main.c of the tree under test, on every run
SRC="${JSPROXY_SRC:-${WINE_INCLUDE:+$WINE_INCLUDE/../dlls/jsproxy/main.c}}"
[ -n "$SRC" ] && [ -f "$SRC" ] || { echo "FAIL  set JSPROXY_SRC (or WINE_INCLUDE) to the tree under test: no main.c"; exit 1; }
{ grep '^#define SG_MUTANT_' "$SRC"
  awk '/SG-SLICE-BEGIN/ {on=1} on; /SG-SLICE-END/ {on=0}' "$SRC"; } > "$T/jsproxy-slice.inc"
grep -q 'global_dispex' "$T/jsproxy-slice.inc" && grep -q 'script_site' "$T/jsproxy-slice.inc" \
    || { echo "FAIL  the objects are not in $SRC"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -I"$T" -o "$T/fastjsproxy-probe.exe" "$HERE/fastjsproxy-probe.c" -lws2_32 -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastjsproxy-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastjsproxy-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
