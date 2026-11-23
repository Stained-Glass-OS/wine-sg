#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Windows Installer's privileges and consent, as Windows does them (patches/sg/9002, 9003).
#
# A standard user (SG_OTHER, default sgconf, in SG_GROUP) of a shared prefix; the elevation broker's
# client is a stand-in (SG_ELEVATE) that records what it was asked and answers "launched".
#   1. msi's Privileged property: empty for the standard user, "1" for the prefix's owner (SYSTEM,
#      elevated) -- Wine said "always admin and privileged"
#   2. msiexec /i of an all-users package in silent mode (/qn): not asked (Windows Installer cannot
#      ask then); the install stops with 1603 and error 1925 in its log, nothing installed
#   3. the same with basic UI (/qb): asked (0590's relaunch through the broker)
#   4. a program's own MsiInstallProduct (a bootstrapper), basic UI, of a package that needs
#      elevation: the broker is asked to run msiexec /i PACKAGE /qb, and the call returns its result
#   5. the same in silent mode (UI level none): not asked
#   6. ALLUSERS=2 with MSIINSTALLPERUSER=1 (test/msipu2.wxs), installed by the standard user: per user
# Mutants: SG_MUTANT_MSI_ALWAYS_PRIVILEGED fails 1 and 2, SG_MUTANT_MSI_NO_1925 fails 2,
# SG_MUTANT_MSIEXEC_QN_RELAUNCH fails 2, SG_MUTANT_MSI_INPROC_NOELEVATE fails 4.
# Needs passwordless `sudo -u $SG_OTHER`, wixl and mingw.
#   WINE=... test/msipriv-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v wixl >/dev/null || { echo "SKIP: wixl missing"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/msipriv.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

cd "$W" && echo hello > hello.txt && wixl -o allusers.msi "$HERE/msiallusers.wxs" 2>/dev/null &&
    wixl -o needadmin.msi "$HERE/msineedadmin.wxs" 2>/dev/null && wixl -o pu2.msi "$HERE/msipu2.wxs" 2>/dev/null ||
    { echo "FAIL  the test packages did not build"; exit 1; }
"$MINGW" -O2 -o "$W/probe.exe" "$HERE/msipriv-probe.c" -lmsi || { echo "FAIL  probe"; exit 1; }
mkdir -m 777 "$W/out"
cat > "$W/elevate" <<EOS
#!/bin/sh
ready=""
if [ "\$1" = --ready ]; then ready=\$2; shift 2; fi
printf '%s\n' "\$*" >> "$W/out/elevate.log"
[ -n "\$ready" ] && printf 0 > "\$ready"
exit 0
EOS
printf 'ready\n' > "$W/elevate.features"
chmod 755 "$W/elevate" "$W/probe.exe"
chmod 644 "$W"/*.msi

mkdir "$PFX"; chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
cp "$W"/*.msi "$PFX/drive_c/"; chmod 664 "$PFX/drive_c/"*.msi
sudo -n -u "$SG_OTHER" mkdir -p "$PFX/drive_c/users/$SG_OTHER/Desktop" "$PFX/drive_c/users/$SG_OTHER/AppData/Local/Temp"
other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
        HOME=/var/tmp SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/elevate.features" "$@"
}
owner() { sg "$SG_GROUP" -c "umask 002; $*"; }
zp() { printf 'Z:%s' "${1//\//\\}"; }
P=$(zp "$W/probe.exe")
asked() { [ -s "$W/out/elevate.log" ]; }

# 1. Privileged
out=$(other "$WINE" "$P" prop 'C:\allusers.msi' Privileged 2>/dev/null | tr -d '\r' | grep '^PROP')
[ "$out" = "PROP Privileged=" ] && pass "a standard user is not Privileged" || fail "standard user's Privileged: '$out'"
out=$(owner "'$WINE' '$P' prop 'C:\\allusers.msi' Privileged" 2>/dev/null | tr -d '\r' | grep '^PROP')
[ "$out" = "PROP Privileged=1" ] && pass "the prefix's owner (SYSTEM, elevated) is Privileged" || fail "owner's Privileged: '$out'"

# 2. silent, all users: not asked, 1925, nothing installed
rm -f "$W/out/elevate.log"
other timeout 120 "$WINE" msiexec /i 'C:\allusers.msi' /qn /l*v "C:\\users\\$SG_OTHER\\qn.log" >/dev/null 2>&1; rc=$?
log=$(sudo -n cat "$PFX/drive_c/users/$SG_OTHER/qn.log" 2>/dev/null | tr -d '\0\r')
asked && fail "2: silent install asked the broker: $(cat "$W/out/elevate.log")" || pass "silent (/qn): not asked"
# (1603: a Unix exit status keeps its low 8 bits, 67)
{ [ "$rc" = 1603 ] || [ "$rc" = 67 ]; } && grep -q 'sufficient privileges to complete this installation for all users' <<<"$log" && pass "...it stops with 1603 and error 1925's message in its log" || fail "2: rc $rc, log: $(grep -ai "error\|1925\|InstallInitialize\|Return value 3" <<<"$log" | head -5 | tr "\n" "|")"
[ ! -e "$PFX/drive_c/Program Files (x86)/SGAllUsers/hello.txt" ] && [ ! -e "$PFX/drive_c/Program Files/SGAllUsers/hello.txt" ] &&
    pass "...and installs nothing" || fail "2: files were installed"

# 3. basic UI: asked
rm -f "$W/out/elevate.log"
other timeout 120 "$WINE" msiexec /i 'C:\allusers.msi' /qb >/dev/null 2>&1
grep -qi 'msiexec' "$W/out/elevate.log" 2>/dev/null && pass "basic UI (/qb): the broker is asked ($(head -1 "$W/out/elevate.log"))" ||
    fail "3: not asked: $(cat "$W/out/elevate.log" 2>/dev/null)"

# 4. a bootstrapper's MsiInstallProduct, basic UI (3): asked to run msiexec /i PACKAGE /qb
rm -f "$W/out/elevate.log"
out=$(other timeout 120 "$WINE" "$P" inproc 'C:\needadmin.msi' 3 2>/dev/null | tr -d '\r' | grep '^INPROC')
log=$(cat "$W/out/elevate.log" 2>/dev/null)
grep -qi 'msiexec.exe' <<<"$log" && grep -q '/i' <<<"$log" && grep -qi 'needadmin.msi' <<<"$log" && grep -q '/qb' <<<"$log" &&
    [ "$out" = "INPROC 0" ] && pass "a program's own MsiInstallProduct asks, as msiexec ($log; $out)" || fail "4: '$out', broker: '$log'"

# 5. silent: not asked
rm -f "$W/out/elevate.log"
other timeout 120 "$WINE" "$P" inproc 'C:\allusers.msi' 2 >/dev/null 2>&1
asked && fail "5: a silent in-process install asked: $(cat "$W/out/elevate.log")" || pass "...not in silent mode (UI level none)"

# 6. ALLUSERS=2 + MSIINSTALLPERUSER=1: per user (no consent, no 1925 for the standard user)
other timeout 120 "$WINE" msiexec /i 'C:\pu2.msi' /qn >/dev/null 2>&1
out=$(other "$WINE" "$P" ctx '{3E4F5A6B-7C8D-4E9F-8A0B-1C2D3E4F5A6B}' 2>/dev/null | tr -d '\r' | grep '^CTX')
[ "$out" = "CTX user" ] && pass "ALLUSERS=2 with MSIINSTALLPERUSER=1 installs per user" || fail "6: $out"
echo "msipriv-gate: $fails failure(s)"
exit $((fails > 0))
