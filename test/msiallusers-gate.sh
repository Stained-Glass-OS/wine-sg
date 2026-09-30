#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package installed for all users (patches/sg/0590). msiexec ran it as the
# user, who may not write to Program Files or HKLM: PuTTY's installer rolled
# back, and installs that got through put their shortcuts in the installing
# account's own Start menu -- installed elevated, SYSTEM's, which nobody sees
# (David: PuTTY added no icons). Now msiexec asks for an administrator (runs
# itself again with "runas") and the shortcuts go to the All Users Start
# menu. A perMachine test package (test/msiallusers.wxs, built with wixl) is
# installed without UI.
#
#   WINE=/opt/wine-sg/bin/wine test/msiallusers-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
command -v wixl >/dev/null || { echo "SKIP: wixl missing (apt install wixl)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msiallusers.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
cd "$T" && echo hello > hello.txt && wixl -o allusers.msi "$HERE/msiallusers.wxs" 2>/dev/null \
    || { echo "FAIL  the test package did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp allusers.msi "$WINEPREFIX/drive_c/"
WINEDEBUG=trace+msiexec timeout 120 "$WINE" msiexec /i 'C:\allusers.msi' /qn > "$T/msiexec.log" 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
rc=0
[ -f "$C/Program Files (x86)/SGAllUsers/hello.txt" ] || [ -f "$C/Program Files/SGAllUsers/hello.txt" ] \
    && echo "PASS  the package is installed" || { echo "FAIL  not installed"; rc=1; }
grep -q 'installing for all users, elevated' "$T/msiexec.log" \
    && echo "PASS  msiexec asks for an administrator (runs itself elevated)" \
    || { echo "FAIL  msiexec did not ask for an administrator"; rc=1; }
if [ -f "$C/ProgramData/Microsoft/Windows/Start Menu/Programs/SGAllUsers/Hello.lnk" ]; then
    echo "PASS  its shortcut is in the All Users Start menu"
else
    echo "FAIL  no shortcut in the All Users Start menu: $(find "$C" -name 'Hello.lnk' 2>/dev/null)"; rc=1
fi
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
