#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# tasklist /m, net user / net localgroup (read only, from the host account database), Win32_PageFileUsage from
# /proc/swaps, and no invented BIOS / baseboard strings (patch 3526).
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-acct-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-fastb-acct.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
pass() { printf "      PASS  %s\n" "$1"; }
fail() { printf "      FAIL  %s\n" "$1"; RC=1; }
check() { if [ "$2" = 0 ]; then pass "$1"; else fail "$1"; fi; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
run() {
    printf '@echo off\r\n%s\r\n' "$1" > "$C/a.bat"
    timeout -s KILL 180 env DISPLAY= "$WINE" cmd /c 'c:\a.bat' 2>&1 </dev/null | tr -d '\r' > "$T/o"
}
me=$(id -un)

echo "== tasklist /m"
run 'tasklist /m ntdll*'
check "lists the loaded ntdll" "$(grep -q 'ntdll.dll' "$T/o"; echo $?)"
check "only the matching module" "$(! grep -q 'kernel32' "$T/o"; echo $?)"
run 'tasklist /m /fo csv /nh /fi "imagename eq cmd.exe"'
check "csv row with a module list" "$(grep -q '^"cmd.exe","[0-9]*","cmd.exe,ntdll.dll' "$T/o"; echo $?)"
run 'tasklist /m /v'
check "/m with /v is a syntax error" "$(grep -q 'Invalid syntax' "$T/o"; echo $?)"
run 'tasklist /m nosuchmodule*'
check "no match says so" "$(grep -q 'No tasks' "$T/o"; echo $?)"

echo "== net user / localgroup"
run 'net user'
check "net user lists $me" "$(grep -q "$me" "$T/o"; echo $?)"
run "net user $me"
check "net user NAME shows the name" "$(grep -q "^User name  *$me" "$T/o"; echo $?)"
run 'net user nosuchaccount1'
check "unknown user" "$(grep -q 'could not be found' "$T/o"; echo $?)"
run 'net localgroup'
check "groups listed" "$(grep -q '^\*Administrators' "$T/o" && grep -q '^\*Users' "$T/o"; echo $?)"
run 'net localgroup Users'
check "Users has $me" "$(grep -q "^$me\$" "$T/o"; echo $?)"
run 'net localgroup nosuchgroup'
check "unknown group" "$(grep -q 'does not exist' "$T/o"; echo $?)"

echo "== Win32_PageFileUsage"
run 'wmic pagefile get Name,AllocatedBaseSize /format:list'
n=$(grep -c '^Name=' "$T/o"); h=$(tail -n +2 /proc/swaps | wc -l)
check "one row per swap area ($h)" "$([ "$n" = "$h" ]; echo $?)"
if [ "$h" -gt 0 ]; then
    first=$(sed -n '2p' /proc/swaps | awk '{print $1}')
    want="Name=Z:$(echo "$first" | sed 's#/#\\#g')"
    check "name is the swap path on Z:" "$(grep -qF "$want" "$T/o"; echo $?)"
fi

echo "== no invented identity strings"
run 'wmic baseboard get Version /format:list'
check "baseboard version is not the made-up 1.0 unless the board says so" \
  "$(v=$(sed -n 's/^Version=//p' "$T/o"); [ "$v" = "$(cat /sys/class/dmi/id/board_version 2>/dev/null)" ]; echo $?)"
echo "== ipconfig /all"
run 'ipconfig /all'
if grep -q 'IPv4 address' "$T/o"; then
    check "subnet mask follows an IPv4 address" "$(grep -q 'Subnet mask [. ]*: [0-9]*\.[0-9]*\.[0-9]*\.[0-9]*' "$T/o"; echo $?)"
fi
check "usage lists the options" "$(run 'ipconfig /?'; grep -q 'flushdns' "$T/o"; echo $?)"

echo "== attrib"
run 'mkdir c:\at\sub & echo x> c:\at\a.txt & echo x> c:\at\sub\b.txt & cd /d c:\at & attrib +r +h a.txt & attrib a.txt'
check "+r +h both applied" "$(grep -q '^A   HR  *c:.at.a.txt' "$T/o"; echo $?)"
run 'cd /d c:\at & attrib +a a.txt'
check "changing a hidden file needs -h" "$(grep -q 'Not resetting hidden file' "$T/o"; echo $?)"
run 'cd /d c:\at & attrib -h -r a.txt & attrib /s /d +a *.txt & attrib /s *.txt'
check "/s reaches the subfolder" "$(grep -q 'at.sub.b.txt' "$T/o"; echo $?)"
run 'attrib /x'
check "invalid switch" "$(grep -q 'Invalid switch - /x' "$T/o"; echo $?)"
run 'cd /d c:\at & attrib nothere.txt'
check "file not found text" "$(grep -q 'File not found - nothere.txt' "$T/o"; echo $?)"

echo "== xcopy /v /o /z"
run 'mkdir c:\xs & echo hello> c:\xs\a.txt & xcopy /v /o /z /i /y c:\xs c:\xd & type c:\xd\a.txt'
check "copied and verified" "$(grep -q '^hello' "$T/o" && grep -q '1 file(s) copied' "$T/o"; echo $?)"

echo "== icacls /restore"
run 'mkdir c:\ic & echo x> c:\ic\a.txt & cd /d c:\ic & icacls a.txt /save c:\ic.sav /q & icacls a.txt /grant Everyone:F /q & icacls c:\ic /restore c:\ic.sav & icacls a.txt'
check "restore reports the file" "$(grep -q 'processed file: c:.ic.a.txt' "$T/o"; echo $?)"
check "the grant is gone again" "$(awk '/processed file/{f=1} f' "$T/o" | grep -q 'Everyone:(RX)'; echo $?)"

echo "== tasklist /fi modules, NetUserGetLocalGroups"
run 'tasklist /fi "modules eq shell32.dll" /nh /fo csv'
check "processes with shell32" "$(grep -q '^"cmd.exe"' "$T/o"; echo $?)"
run "net user $me"
check "net user shows local group memberships" "$(grep -q 'Local Group Memberships .*\*Users' "$T/o"; echo $?)"
echo "== robocopy /maxage /minage"
run 'mkdir c:\rs & echo a> c:\rs\new.txt & echo b> c:\rs\old.txt & robocopy c:\rs c:\rd /maxage:1800 /njh /njs /ndl & dir /b c:\rd'
check "files younger than the limit are copied" "$(grep -q '^new.txt' "$T/o"; echo $?)"
run 'robocopy c:\rs c:\rd2 /minage:1800 /njh /njs /ndl & dir /b c:\rd2'
check "nothing is old enough" "$(! grep -q 'new.txt' "$T/o"; echo $?)"
exit $RC
