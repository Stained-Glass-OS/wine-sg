#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# .js and .wsf files open with the script host (patches/sg/9622):
# test/scriptassoc.vbs checks the JSFile and WSFFile verbs and opens a .js and a
# .wsf through ShellExecute (WScript.Shell.Run), each writing a marker file.
# Headless (the scripts show nothing).
#
#   WINE=/opt/wine-sg/bin/wine test/scriptassoc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# No mutant macro: the change is registration data; the gate fails on 10.0-891.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-scriptassoc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/scriptassoc.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo scriptassoc.vbs 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 env DISPLAY= "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
