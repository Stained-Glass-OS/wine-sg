#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package installed per user that needs elevation (patches/sg/9001). Windows Installer asks for
# an administrator for every install unless the package is per user (ALLUSERS empty) AND says
# elevated privileges are not required (bit 3 of the summary's word count) -- and never in silent
# mode (/qn), where it cannot ask. Mixxx's and Bespoke Synth's MSIs (WiX, ALLUSERS unset, bit 3
# clear, INSTALLDIR in Program Files) ran as the person and failed 1603; msiexec now runs itself
# again with "runas" (the consent prompt) for them, as 0590 does for an all-users package.
#   1. test/msineedadmin.wxs (per user, bit 3 clear), basic UI (/qb): msiexec asks; installed
#   2. the same in silent mode (/qn): not asked
#   3. test/msiperuser.wxs (per user, bit 3 set), /qb: not asked
#   4. uninstalling the installed product by its product code (/x {...} /qb): asked
#   5. mutant SG_MUTANT_MSI_PERUSER_NOELEVATE (programs/msiexec) fails 1 (and 4)
# "asked" is msiexec's trace "installing for all users, elevated" (its runas relaunch).
#
#   WINE=/opt/wine-sg/bin/wine test/msineedadmin-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v wixl >/dev/null || { echo "SKIP: wixl missing (apt install wixl)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msineedadmin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
cd "$T" && echo hello > hello.txt && wixl -o needadmin.msi "$HERE/msineedadmin.wxs" 2>/dev/null &&
    wixl -D Code=4b2c1d0e-9f8a-4b7c-8d6e-5f4a3b2c1d0e -D N=9 -D Upgrade=1c2d3e4f-5a6b-4c7d-8e9f-0a1b2c3d4e5f \
         -D Comp=2d3e4f5a-6b7c-4d8e-9f0a-1b2c3d4e5f6a -o peruser.msi "$HERE/msiperuser.wxs" 2>/dev/null \
    || { echo "FAIL  the test packages did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp needadmin.msi peruser.msi "$WINEPREFIX/drive_c/"
C="$WINEPREFIX/drive_c"
rc=0
asked() { grep -q 'installing for all users, elevated' "$1"; }
run() { # log args...
    log=$1; shift
    WINEDEBUG=trace+msiexec timeout 120 "$WINE" msiexec "$@" > "$log" 2>&1
    "$WINESERVER" -w
}
run "$T/1.log" /i 'C:\needadmin.msi' /qb
asked "$T/1.log" && echo "PASS  a per-user package without \"no elevation needed\": msiexec asks for an administrator" \
    || { echo "FAIL  1: msiexec did not ask"; rc=1; }
[ -f "$C/Program Files (x86)/SGNeedsAdmin/hello.txt" ] || [ -f "$C/Program Files/SGNeedsAdmin/hello.txt" ] \
    && echo "PASS  ...and it is installed in Program Files" || { echo "FAIL  1: not installed"; rc=1; }
run "$T/4.log" /x '{7D1E2F30-4A5B-4C6D-8E7F-90A1B2C3D4E5}' /qb
asked "$T/4.log" && echo "PASS  uninstalling that product by its code asks too" || { echo "FAIL  4: not asked"; rc=1; }
run "$T/2.log" /i 'C:\needadmin.msi' /qn
asked "$T/2.log" && { echo "FAIL  2: asked in silent mode"; rc=1; } \
    || echo "PASS  in silent mode (/qn) it is not asked, as Windows Installer cannot ask then"
run "$T/3.log" /i 'C:\peruser.msi' /qb
asked "$T/3.log" && { echo "FAIL  3: a no-elevation per-user package was asked about"; rc=1; } \
    || echo "PASS  a per-user package that says it needs no elevation is not asked about"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
