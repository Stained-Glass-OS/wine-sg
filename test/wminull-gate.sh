#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemdisp method calls from scripts (patches/sg/9616): Null for a parameter is
# "no value", and an SWbemObject for an object parameter goes in as its WMI
# object. test/wminull.vbs calls Win32_Process.Create(cmd, Null, config, pid)
# with a Win32_ProcessStartup instance, the way Microsoft's samples do. Headless.
#
#   WINE=/opt/wine-sg/bin/wine test/wminull-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/wbemdisp/locator.c): SG_MUTANT_WBEM_NULL_ARG_CONVERTED, SG_MUTANT_WBEM_OBJECT_ARG_WRAPPED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wminull.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/wminull.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo wminull.vbs 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 env DISPLAY= "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
