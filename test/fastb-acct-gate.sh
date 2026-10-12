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
exit $RC
