#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows Script Host argument collections (patches/sg/9619): test/wshargs.vbs
# runs under cscript with plain, /name:value, /switch and quoted arguments and
# checks For Each over WScript.Arguments, .Named (Item, Exists, Count, For Each)
# and .Unnamed. Headless.
#
#   WINE=/opt/wine-sg/bin/wine test/wshargs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (programs/wscript/arguments.c): SG_MUTANT_WSH_NO_ARG_COLLECTIONS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wshargs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/wshargs.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo wshargs.vbs first '/Path:C:\x\y' /v second "with space" /n: 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 env DISPLAY= "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
