#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows Script Files (patches/sg/9620): cscript runs test/wsfjobs.wsf -- a
# package of jobs with a VBScript src file, JScript and VBScript blocks calling
# each other, an <object> and a <reference> (ADO constants) -- picks jobs with
# //job:, and stops with the error's line in the .wsf. Wine's cscript ran
# nothing for a .wsf. Headless.
#
#   WINE=/opt/wine-sg/bin/wine test/wsfjobs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (programs/wscript/main.c): SG_MUTANT_WSH_NO_WSF.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wsfjobs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/wsfjobs.wsf" "$HERE/wsfjobs-lib.vbs" "$C/"
printf '<job>\r\n<script language="VBScript">\r\nWScript.Echo "before"\r\nx = 1 / 0\r\n</script>\r\n</job>\r\n' > "$C/wsferr.wsf"
run() { (cd "$C" && timeout -s KILL 120 "$WINE" cscript.exe //nologo "$@" 2>&1 </dev/null | tr -d '\r'); }
RC=0
out=$(run wsfjobs.wsf arg1)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || { echo "FAIL  the main job"; RC=1; }
out=$(run //job:second wsfjobs.wsf)
[ "$out" = "JOB second ran" ] && echo "PASS  //job:second runs that job" || { echo "FAIL  //job:second: $out"; RC=1; }
out=$(run //job:xmljob wsfjobs.wsf)
[ "$out" = "JOB xmljob True" ] && echo "PASS  a CDATA block" || { echo "FAIL  //job:xmljob: $out"; RC=1; }
out=$(run //job:nosuch wsfjobs.wsf)
printf '%s\n' "$out" | grep -qi 'unable to find job' && echo "PASS  an unknown job is reported" || { echo "FAIL  //job:nosuch: $out"; RC=1; }
out=$(run wsferr.wsf)
printf '%s\n' "$out" | grep -q '^before$' && printf '%s\n' "$out" | grep -q 'wsferr.wsf(4, ' && echo "PASS  an error names its line in the .wsf" || { echo "FAIL  error line: $out"; RC=1; }
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
