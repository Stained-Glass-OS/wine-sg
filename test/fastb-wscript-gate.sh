#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wscript (patch 3574): WScript.Timeout is kept, StdIn/StdOut/StdErr are the standard streams.
#   WINE=/path/to/build/wine test/fastb-wscript-gate.sh      SG_XVFB_N=261 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-wscript.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# the build tree's hosts replace the prefix's copies
for a in wscript cscript; do
  [ -f "$BUILD/programs/$a/x86_64-windows/$a.exe" ] && cp "$BUILD/programs/$a/x86_64-windows/$a.exe" "$WINEPREFIX/drive_c/windows/system32/$a.exe"
done
cat > "$WINEPREFIX/drive_c/t.vbs" <<'EOS'
WScript.Timeout = 7
WScript.StdOut.WriteLine "timeout=" & WScript.Timeout
WScript.StdErr.WriteLine "to-stderr"
Dim line
line = WScript.StdIn.ReadLine
WScript.StdOut.WriteLine "read=" & line
On Error Resume Next
WScript.Timeout = -1
If Err.Number <> 0 Then WScript.StdOut.WriteLine "negative-refused"
EOS
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
echo hello | timeout -s KILL 120 "$WINE" cscript.exe //nologo t.vbs 2>&1 | tr -d '\r' > "$T/out.txt"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/out.txt"
fail=0
for want in "timeout=7" "to-stderr" "read=hello" "negative-refused"; do
  grep -qx "$want" "$T/out.txt" || { echo "FAIL  missing line: $want"; fail=1; }
done
[ $fail = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $fail
