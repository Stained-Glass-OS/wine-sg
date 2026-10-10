#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32 special folder batch (patches/sg/2017): test/sfparse-probe.c
# checks ParseDisplayName names and pchEaten, shell extension attributes, and
# My Computer attributes through the desktop and directly.
#
#   WINE=/opt/wine-sg/bin/wine test/sfparse-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/shell32): SG_MUTANT_PARSE_NAMES (shfldr_fs.c), EATEN
# (shfldr_mycomp.c), MYCOMP_DIRECT, EMPTY_PIDL (shfldr_mycomp.c), MYCOMP_CANLINK
# (shfldr_desktop.c), GUID_DEFAULT (shlfolder.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sfparse.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/sfparse-probe.exe" "$HERE/sfparse-probe.c" \
    -lole32 -loleaut32 -luuid -lshell32 -lshlwapi \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sfparse-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" sfparse-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  probe did not finish (crashed?)'
exit 1
