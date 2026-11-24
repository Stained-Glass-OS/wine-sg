#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WshExec.Status (patches/sg/9010): test/wshexecstatus.vbs runs children under
# WScript.Shell.Exec that exit 3, 0 and -1, one that runs 1.5 s then exits 3
# (Status must be WshRunning, then WshFinished: the Windows loop
# "Do Until oExec.Status = 1" must end), and a terminated one. Windows says
# WshFinished for any ended child; Wine said WshFailed for a nonzero exit.
# Headless.
#
#   WINE=/opt/wine-sg/bin/wine test/wshexecstatus-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (dlls/wshom.ocx/shell.c): SG_MUTANT_WSHEXEC_FAILED_ON_EXIT_CODE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wshexecstatus.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/wshexecstatus.vbs" "$HERE/wshexec-child.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo wshexecstatus.vbs 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 env DISPLAY= "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
