#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wmic: the alias list, WHERE, GET / LIST / CALL, the table, list, value and csv formats, /OUTPUT, /NAMESPACE
# (patches/sg/2461), run through cmd.exe.
#
#   WINE=/opt/wine-sg/bin/wine test/wmic-gate.sh
# Mutants (programs/wmic/main.c): SG_MUTANT_WMIC_SORT (columns in the order asked), SG_MUTANT_WMIC_BOOL (booleans as
# numbers), SG_MUTANT_WMIC_WHERE (WHERE ignored), SG_MUTANT_WMIC_CSVNODE (no Node column).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-wmic.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

pass() { printf "      PASS  %s\n" "$1"; }
fail() { printf "      FAIL  %s\n" "$1"; RC=1; }
check() { # check NAME CONDITION-exit-status
    if [ "$2" = 0 ]; then pass "$1"; else fail "$1"; fi
}

cat > "$T/sleeper.c" <<'EOF'
#include <windows.h>
int main(void) { Sleep(60000); return 0; }
EOF
x86_64-w64-mingw32-gcc -O1 -o "$T/sleeper.exe" "$T/sleeper.c" || { echo "FAIL  sleeper did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"

# run one wmic command line through cmd.exe, as a person would type it; the answer is in $T/out (stdout) and $T/err
wm() {
    # in a batch file a percent sign is doubled (it is the person's keyboard, not a batch file, that wmic is meant for)
    printf '@echo off\r\nwmic %s\r\n' "$(printf '%s' "$1" | sed 's/%/%%/g')" > "$C/wm.bat"
    timeout -s KILL 180 env DISPLAY= "$WINE" cmd /c 'c:\wm.bat' 2> "$T/err" </dev/null | cat > "$T/out"
    tr -d '\r' < "$T/out" > "$T/o"
    tr -d '\r' < "$T/err" > "$T/e"
}

echo "== get, formats"
wm 'os get version /value'
check "value: a blank line, Version=, two blank lines" "$([ "$(sed -n 1p "$T/o")" = "" ] && sed -n 2p "$T/o" | grep -q '^Version=10\.0\.[0-9]*$' && [ "$(sed -n 3p "$T/o")" = "" ] && [ "$(sed -n 4p "$T/o")" = "" ]; echo $?)"
wm 'os get version,caption'
check "table: alphabetical columns" "$(sed -n 1p "$T/o" | grep -q '^Caption  *Version  *$'; echo $?)"
check "table: the header ends in two blanks" "$(sed -n 1p "$T/o" | grep -q '  $'; echo $?)"
check "table: one row, then a blank line" "$([ "$(sed -n 2p "$T/o" | cut -c1-9)" = "Microsoft" ] && [ "$(sed -n 3p "$T/o")" = "" ]; echo $?)"
# the second column starts where the header's does
h=$(sed -n 1p "$T/o" | awk '{print index($0,"Version")}'); r=$(sed -n 2p "$T/o" | awk '{print index($0,"10.0.")}')
check "table: columns line up" "$([ "$h" = "$r" ] && [ -n "$h" ]; echo $?)"
wm 'os get caption /format:list'
check "list: Caption=" "$(sed -n 2p "$T/o" | grep -q '^Caption=Microsoft'; echo $?)"
wm 'logicaldisk get size,deviceid /format:csv'
node=$(sed -n 2p "$T/o" | cut -d, -f1)
check "csv: a blank line first" "$([ "$(sed -n 1p "$T/o")" = "" ]; echo $?)"
check "csv: Node,DeviceID,Size" "$([ "$(sed -n 2p "$T/o")" = "Node,DeviceID,Size" ]; echo $?)"
check "csv: rows start with the node" "$(sed -n 3p "$T/o" | grep -q '^[A-Za-z0-9_-]*,[A-Z]:,[0-9]*$'; echo $?)"
wm 'service where name="Eventlog" get name,started,state'
check "booleans are TRUE / FALSE" "$(sed -n 2p "$T/o" | grep -q '^Eventlog  *\(TRUE\|FALSE\)  *\(Running\|Stopped\)  *$'; echo $?)"
wm 'os list brief'
check "list brief: BuildNumber .. Version" "$(sed -n 1p "$T/o" | grep -q '^BuildNumber  *Caption  *FreePhysicalMemory  *Status  *TotalVirtualMemorySize  *TotalVisibleMemorySize  *Version  *$'; echo $?)"
wm 'bios list full'
check "list full is name=value lines" "$(grep -q '^Manufacturer=' "$T/o" && grep -q '^SerialNumber=' "$T/o"; echo $?)"

echo "== where"
wm 'process where "name='"'"'services.exe'"'"'" get name,processid'
check "WHERE in quotes: one process" "$([ "$(grep -c services.exe "$T/o")" = 1 ]; echo $?)"
wm 'process where name="services.exe" get processid,name'
check "WHERE name=\"x\": columns sorted, not as asked" "$(sed -n 1p "$T/o" | grep -q '^Name  *ProcessId  *$'; echo $?)"
wm 'process where name="no-such-process.exe" get processid'
check "no instance: the message" "$(grep -q 'No Instance(s) Available.' "$T/e"; echo $?)"
check "no instance: nothing on stdout" "$([ ! -s "$T/o" ]; echo $?)"
wm 'logicaldisk where "deviceid like '"'"'%:'"'"'" get deviceid'
check "WHERE with LIKE" "$(grep -q '^C:' "$T/o"; echo $?)"

echo "== errors"
wm 'nonesuch get name'
check "unknown alias" "$(grep -q 'Alias not found' "$T/e"; echo $?)"
wm 'os get nonesuch'
check "unknown property" "$(grep -q 'Invalid GET Expression.' "$T/e"; echo $?)"
wm 'os where nonesuch=1 get caption'
check "bad WHERE: an error" "$(grep -q 'ERROR:' "$T/e"; echo $?)"
wm 'os get caption /format:nonesuch'
check "unknown format" "$(grep -q 'Invalid XSL format' "$T/e"; echo $?)"
wm '/nonesuch os get caption'
check "unknown global switch" "$(grep -q 'Invalid Global Switch.' "$T/e"; echo $?)"
wm '/node:somewhere-else os get caption'
check "another machine" "$(grep -q 'RPC server is unavailable' "$T/e"; echo $?)"

echo "== path, namespace, output"
wm 'path win32_bios get name'
check "path <class>" "$(sed -n 2p "$T/o" | grep -q 'BIOS'; echo $?)"
wm '/namespace:\\root\cimv2 path win32_operatingsystem get version /value'
check "/namespace" "$(grep -q '^Version=' "$T/o"; echo $?)"
wm 'path win32_nonesuch get x'
check "unknown class" "$(grep -q 'Invalid class' "$T/e"; echo $?)"
rm -f "$C/wmout.txt"
wm '/output:c:\wmout.txt os get version'
check "/output leaves standard output empty" "$([ ! -s "$T/o" ]; echo $?)"
check "/output: a Unicode file with a byte order mark" "$([ "$(head -c2 "$C/wmout.txt" | od -An -tx1 | tr -d ' ')" = "fffe" ]; echo $?)"
check "/output: the text" "$(iconv -f UTF-16 -t UTF-8 "$C/wmout.txt" 2>/dev/null | tr -d '\r' | grep -q '^Version  *$'; echo $?)"
wm '/append:c:\wmout.txt os get version'
check "/append: twice the text" "$([ "$(iconv -f UTF-16 -t UTF-8 "$C/wmout.txt" 2>/dev/null | grep -c '^Version')" = 2 ]; echo $?)"

echo "== call"
# one batch file: a process made by wmic goes with the console it was made on
SL="Z:$(echo "$T" | sed 's#/#\\#g')\\sleeper.exe"
printf '@echo off\r\nwmic process call create "%s"\r\necho ====1\r\nwmic process where name="sleeper.exe" get name,processid\r\necho ====2\r\nwmic process where name="sleeper.exe" call terminate\r\necho ====3\r\nping -n 2 127.0.0.1 >nul\r\nwmic process where name="sleeper.exe" get name\r\necho ====4\r\nwmic process call nonesuch\r\n' "$SL" > "$C/wm.bat"
timeout -s KILL 180 env DISPLAY= "$WINE" cmd /c 'c:\wm.bat' 2> "$T/err" </dev/null | cat > "$T/out"
tr -d '\r' < "$T/out" > "$T/o"; tr -d '\r' < "$T/err" > "$T/e"
awk '/^====1/{exit} {print}' "$T/o" > "$T/o1"
awk '/^====1/{f=1;next} /^====2/{exit} f{print}' "$T/o" > "$T/o2"
awk '/^====2/{f=1;next} /^====3/{exit} f{print}' "$T/o" > "$T/o3"
awk '/^====3/{f=1;next} /^====4/{exit} f{print}' "$T/o" > "$T/o4"
check "call create: executing" "$(grep -q '^Executing (Win32_Process)->create()' "$T/o1"; echo $?)"
check "call create: successful" "$(grep -q '^Method execution successful.' "$T/o1"; echo $?)"
pid=$(grep -o 'ProcessId = [0-9]*' "$T/o1" | grep -o '[0-9]*$')
check "call create: ProcessId and ReturnValue = 0" "$([ -n "$pid" ] && grep -q 'ReturnValue = 0;' "$T/o1"; echo $?)"
check "the new process is there, with its id" "$(grep -q "^sleeper.exe  *$pid  *$" "$T/o2"; echo $?)"
check "call terminate: the path of the instance" "$(grep -q "^Executing (.*Win32_Process.Handle=\"$pid\")->terminate()" "$T/o3"; echo $?)"
check "call terminate: ReturnValue = 0" "$(grep -q 'ReturnValue = 0;' "$T/o3"; echo $?)"
check "it is gone" "$(grep -q 'No Instance' "$T/e"; echo $?)"
check "unknown method" "$(grep -q 'Invalid method' "$T/e"; echo $?)"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
