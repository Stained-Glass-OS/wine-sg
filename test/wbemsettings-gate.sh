#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Win32_Environment, Win32_StartupCommand and Win32_TimeZone, a class with no instance, property enumeration
# (patches/sg/2462); the probe 64- and 32-bit, the classes through wmic.
#
#   WINE=/opt/wine-sg/bin/wine test/wbemsettings-gate.sh
# Mutants (wbemprox/builtin.c): SG_MUTANT_WBEM_ENV_USER (the user's variables left out), SG_MUTANT_WBEM_STARTUP_HKCU
# (the user's Run key left out), SG_MUTANT_WBEM_TZ_BIAS (the bias with the wrong sign).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-wbemsettings.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
pass() { printf "      PASS  %s\n" "$1"; }
fail() { printf "      FAIL  %s\n" "$1"; RC=1; }
check() { if [ "$2" = 0 ]; then pass "$1"; else fail "$1"; fi; }

for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/wbemsettings-probe.c" -lole32 -loleaut32 -luuid -lwbemuuid \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"

for a in x86_64 i686; do
    echo "== probe $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done

wm() {
    printf '@echo off\r\nwmic %s\r\n' "$(printf '%s' "$1" | sed 's/%/%%/g')" > "$C/wm.bat"
    timeout -s KILL 180 env DISPLAY= "$WINE" cmd /c 'c:\wm.bat' 2> "$T/err" </dev/null | cat > "$T/out"
    tr -d '\r' < "$T/out" > "$T/o"; tr -d '\r' < "$T/err" > "$T/e"
}

echo "== no startup commands"
wm 'startup get name'
check "a class with no instance says so (not Invalid class)" "$(grep -q 'No Instance(s) Available.' "$T/e"; echo $?)"

echo "== Win32_Environment"
cat > "$C/setup.bat" <<'EOF'
@echo off
reg add "HKCU\Environment" /v SgProbeUser /d hello /f >nul
reg add "HKCU\Environment" /v SgProbeExpand /t REG_EXPAND_SZ /d "%%SystemRoot%%\sgprobe" /f >nul
reg add "HKLM\System\CurrentControlSet\Control\Session Manager\Environment" /v SgProbeSystem /d world /f >nul
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Run" /v SgProbeRunM /d "C:\sgprobe\machine.exe -m" /f >nul
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v SgProbeRunU /d "C:\sgprobe\user.exe -u" /f >nul
mkdir "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup" 2>nul
echo x > "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\sgprobe-startup.cmd"
EOF
timeout -s KILL 180 env DISPLAY= "$WINE" cmd /c 'c:\setup.bat' >/dev/null 2>&1 </dev/null
wm 'environment where name="SgProbeUser" get name,systemvariable,username,variablevalue /format:list'
check "the user's variable" "$(grep -q '^VariableValue=hello$' "$T/o" && grep -q '^SystemVariable=FALSE$' "$T/o" && grep -q '^UserName=.*\\.*$' "$T/o"; echo $?)"
wm 'environment where name="SgProbeSystem" get name,systemvariable,username,variablevalue /format:list'
check "the machine's variable: <SYSTEM>, TRUE" "$(grep -q '^VariableValue=world$' "$T/o" && grep -q '^SystemVariable=TRUE$' "$T/o" && grep -q '^UserName=<SYSTEM>$' "$T/o"; echo $?)"
wm 'environment where name="SgProbeExpand" get variablevalue /value'
check "a REG_EXPAND_SZ value is shown as stored" "$(grep -q '^VariableValue=%SystemRoot%\\sgprobe$' "$T/o"; echo $?)"
wm 'environment where name="SgProbeUser" get caption /value'
check "Caption is user\\name" "$(grep -q '^Caption=.*\\SgProbeUser$' "$T/o"; echo $?)"
wm 'environment where "systemvariable=true and username='"'"'<SYSTEM>'"'"'" get name'
check "WHERE on a boolean and a string" "$(grep -q '^SgProbeSystem' "$T/o" && ! grep -q SgProbeUser "$T/o"; echo $?)"

echo "== Win32_StartupCommand"
wm 'startup get command,location,name,user /format:csv'
check "HKLM Run entry: user Public" "$(grep -q ',C:.sgprobe.machine.exe -m,HKLM.SOFTWARE.Microsoft.Windows.CurrentVersion.Run,SgProbeRunM,Public$' "$T/o"; echo $?)"
check "HKCU Run entry: location under HKU\\<sid>, user domain\\name" "$(grep -q ',C:.sgprobe.user.exe -u,HKU.S-1-[0-9-]*.SOFTWARE.Microsoft.Windows.CurrentVersion.Run,SgProbeRunU,[^,]*.[^,]*$' "$T/o"; echo $?)"
check "a Startup folder entry" "$(grep -q 'sgprobe-startup.cmd,Startup,sgprobe-startup.cmd,' "$T/o"; echo $?)"
wm 'startup where name="SgProbeRunM" get command /value'
check "WHERE on a name" "$(grep -q '^Command=C:.sgprobe.machine.exe -m$' "$T/o"; echo $?)"

echo "== Win32_TimeZone"
wm 'timezone get bias,caption,standardname,daylightname,daylightbias,standardbias /format:list'
bias=$(sed -n 's/^Bias=//p' "$T/o")
check "Bias is minutes east of UTC, within a day" "$([ -n "$bias" ] && [ "$bias" -ge -720 ] && [ "$bias" -le 840 ]; echo $?)"
check "Caption and StandardName are not empty" "$(grep -q '^Caption=.' "$T/o" && grep -q '^StandardName=.' "$T/o"; echo $?)"
# the host's own offset (minutes east) is the standard bias, or an hour more in summer
host=$(date +%z | awk '{ sign = (substr($0,1,1) == "-") ? -1 : 1; h = substr($0,2,2); m = substr($0,4,2); print sign * (h * 60 + m) }')
check "Bias is the host's standard offset (or an hour less than its summer one)" "$([ "$host" = "$bias" ] || [ "$host" = "$((bias + 60))" ]; echo $?)"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
