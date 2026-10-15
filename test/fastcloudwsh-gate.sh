#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cscript: WScript.StdIn/StdOut/StdErr, WScript.GetObject, unhandled script errors (stderr text, exit code), missing script file.
#
#   WINE=/opt/wine-sg/bin/wine test/fastcloudwsh-gate.sh   (mutants SG_MUTANT_WSH_*)
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastcloudwsh.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cat > "$C/std.vbs" <<'EOS'
WScript.StdOut.WriteLine "out1"
WScript.StdErr.WriteLine "err1"
Dim l
l = WScript.StdIn.ReadLine
WScript.StdOut.Write "got:" & l
WScript.StdOut.WriteLine ""
EOS
cat > "$C/rt.vbs" <<'EOS'
WScript.Echo "before"
Dim x
x = 1 / 0
WScript.Echo "after"
EOS
printf 'WScript.Echo "never"\r\nx = = 1\r\n' > "$C/syn.vbs"
cat > "$C/obj.vbs" <<'EOS'
Dim o, n
Set o = WScript.GetObject("new:{0D43FE01-F093-11CF-8940-00A0C9054228}")
WScript.Echo "fso:" & o.GetFileName("C:\a\b.txt")
On Error Resume Next
Err.Clear
Set o = WScript.GetObject("")
WScript.Echo "empty:" & (Err.Number <> 0)
Err.Clear
Set o = WScript.GetObject("", "No.Such.Class")
WScript.Echo "noclass:" & (Err.Number <> 0)
EOS
cat > "$C/quit.vbs" <<'EOS'
WScript.Quit 3
EOS
printf 'hello world\r\n' > "$C/in.txt"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$C"
w() { timeout -s KILL 120 "$WINE" cscript.exe //nologo "\$@"; }
w std.vbs < in.txt > "$T/std.out" 2> "$T/std.err"; echo \$? > "$T/std.rc"
w rt.vbs </dev/null > "$T/rt.out" 2> "$T/rt.err"; echo \$? > "$T/rt.rc"
w syn.vbs </dev/null > "$T/syn.out" 2> "$T/syn.err"; echo \$? > "$T/syn.rc"
w missing.vbs </dev/null > "$T/mis.out" 2> "$T/mis.err"; echo \$? > "$T/mis.rc"
w obj.vbs </dev/null > "$T/obj.out" 2> "$T/obj.err"; echo \$? > "$T/obj.rc"
w quit.vbs </dev/null > "$T/q.out" 2> "$T/q.err"; echo \$? > "$T/q.rc"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
fails=0
chk() { # desc, condition-result
    if [ "$2" = 0 ]; then printf "PASS  %s\n" "$1"; else printf "FAIL  %s\n" "$1"; fails=$((fails+1)); fi
}
t() { tr -d '\r' < "$1" | grep -v "^X connection"; }
[ "$(t $T/std.out)" = "out1
got:hello world" ]; chk "StdOut/StdIn: $(t $T/std.out | tr '\n' '|')" $?
[ "$(t $T/std.err)" = "err1" ]; chk "StdErr separate from StdOut: $(t $T/std.err | tr '\n' '|')" $?
[ "$(cat $T/std.rc)" = 0 ]; chk "std.vbs exit 0 ($(cat $T/std.rc))" $?
[ "$(t $T/rt.out)" = "before" ]; chk "runtime error: output before the error only" $?
[ "$(t $T/rt.err)" = 'C:\rt.vbs(3, 1) Microsoft VBScript runtime error: Division by zero' ]; chk "runtime error text on stderr: $(t $T/rt.err)" $?
[ "$(cat $T/rt.rc)" = 1 ]; chk "runtime error exit code 1 ($(cat $T/rt.rc))" $?
[ -z "$(t $T/syn.out)" ]; chk "syntax error: nothing ran" $?
[ "$(t $T/syn.err)" = 'C:\syn.vbs(2, 5) Microsoft VBScript compilation error: Syntax error' ]; chk "syntax error text: $(t $T/syn.err)" $?
[ "$(cat $T/syn.rc)" = 1 ]; chk "syntax error exit code 1 ($(cat $T/syn.rc))" $?
[ "$(t $T/mis.err)" = 'Input Error: Can not find script file "C:\missing.vbs".' ]; chk "missing file: $(t $T/mis.err)" $?
[ "$(cat $T/mis.rc)" = 1 ]; chk "missing file exit code 1" $?
[ "$(t $T/obj.out)" = "fso:b.txt
empty:True
noclass:True" ]; chk "WScript.GetObject: $(t $T/obj.out | tr '\n' '|')" $?
[ "$(cat $T/q.rc)" = 3 ]; chk "Quit 3 exit code ($(cat $T/q.rc))" $?
if [ $fails = 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL"; exit 1
