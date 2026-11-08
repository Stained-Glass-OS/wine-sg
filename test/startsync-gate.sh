#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A package the machine has is in the Start menu of every person, and the shell runs Active Setup
# (patches/sg/7709). eM Client's MSI, run as SYSTEM by the Store, deploys its MSIX for the machine,
# deletes "eM Client.lnk" from the Start menus (its older versions' shortcut), and leaves the rest
# to Active Setup ("MailClient.exe --install-msix" once per person): nobody had it in Start, and
# the Store's Open found nothing (qa4, 2026-11-07). On Windows the package's entries are not files
# an installer can delete, and the shell runs Active Setup at sign-in.
#
# On a shared system prefix (owner = SYSTEM) with a second Unix user SG_OTHER (default sgconf):
#   setup     SYSTEM adds package A for the machine and deletes its all-users shortcut, as eM
#             Client's MSI; Active Setup has a component with a StubPath (64-bit view), one only
#             in the 32-bit view, one with IsInstalled=0, and one the person has at its version
#   sign-in   SG_OTHER's shell: the Active Setup stubs run once (not the uninstalled one, not the
#             one they have), their HKCU keys get the Version; A has a shortcut in SG_OTHER's own
#             Start menu (the package's program, its AUMID)
#   live      still signed in: SYSTEM adds package B and deletes its shortcut -- B is in SG_OTHER's
#             Start menu within seconds; SYSTEM removes B -- its shortcut goes
#   again     the shell started again in the sign-in runs no stub again; the next sign-in neither;
#             a higher machine Version runs it again
#   back      A's all-users shortcut back: SG_OTHER's copy goes (one entry, not two)
#   aumid     SG_OTHER starts A by AUMID and by shell:AppsFolder, with the package's identity
#   WINE=... test/startsync-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
XN=${XN:-273}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" python3 openssl osslsigncode Xvfb; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null ||
    { echo "SKIP: no second user $SG_OTHER (sudo -n -u $SG_OTHER)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-startsync.XXXXXX)
chmod 755 "$T"
PFX=$T/prefix
B=$T/b
export WINEPREFIX=$PFX WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
    LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY WAYLAND_DISPLAY
XPID=
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    set +e
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" "$WINESERVER" -k 2>/dev/null
    "$WINESERVER" -k 2>/dev/null; sleep 1
    [ -n "$XPID" ] && kill "$XPID" 2>/dev/null
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$B" "$T/userhome"
chgrp "$SG_GROUP" "$T/userhome"; chmod 2770 "$T/userhome"

pubid() {
    python3 -c "
import hashlib,sys
h=hashlib.sha256(sys.argv[1].encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))" "$1"
}
PUB='CN=Stained Glass Test Publisher'
ID=$(pubid "$PUB")
FAM_A="StainedGlass.SyncA_$ID"; FULL_A="StainedGlass.SyncA_1.0.0.0_x64__$ID"
FULL_B="StainedGlass.SyncB_1.0.0.0_x64__$ID"

"$MINGW" -O2 -o "$B/msixapps-app.exe" "$HERE/msixapps-app.c" &&
"$MINGW" -O2 -o "$B/sparse-probe.exe" "$HERE/sparse-probe.c" -lruntimeobject -lshlwapi -lole32 -luuid &&
"$MINGW" -O2 -municode -o "$B/msixapps-probe.exe" "$HERE/msixapps-probe.c" -lole32 -lshell32 -luuid -lshlwapi -lgdi32 -luser32 &&
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "the probes did not build"; exit 1; }

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" -subj "/$PUB" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
for p in A B; do
    python3 "$HERE/mkmsix.py" package "$B/$p.msix" --name "StainedGlass.Sync$p" --version 1.0.0.0 \
        --display-name "Sync $p" --app "App:msixapps-app.exe::Sync $p app" --file "$B/msixapps-app.exe:msixapps-app.exe" &&
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/$p.msix" -out "$B/$p-signed.msix" \
        >/dev/null 2>&1 || { fail "package $p could not be made"; exit 1; }
done

# ---- the shared system prefix -----------------------------------------------------------
mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
C=$PFX/drive_c
cp "$B/sparse-probe.exe" "$B/msixapps-probe.exe" "$B/trust-probe.exe" "$B/A-signed.msix" "$B/B-signed.msix" "$C/"
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
mkdir -p "$C/m"
chmod -R g+rwX "$PFX" 2>/dev/null
sudo -n -u "$SG_OTHER" mkdir -p "$C/users/$SG_OTHER/Desktop" "$C/users/$SG_OTHER/Documents" \
    "$C/users/$SG_OTHER/AppData/Roaming" "$C/users/$SG_OTHER/AppData/Local/Temp"

owner() { (cd "$C" && sg "$SG_GROUP" -c "umask 002; timeout -s KILL 180 '$WINE' $(printf "'%s' " "$@")" 2>/dev/null </dev/null | tr -d '\r'); }
user() { (cd "$C" && sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="$WINEDLLOVERRIDES" \
              HOME="$T/userhome" DISPLAY=":$XN" timeout -s KILL 180 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
count() { [ -f "$C/m/$1.txt" ] && grep -c x "$C/m/$1.txt" || echo 0; }
wait_for() {   # wait_for SECONDS TEST...
    local n=$1; shift
    while [ "$n" -gt 0 ]; do "$@" && return 0; sleep 1; n=$((n - 1)); done
    return 1
}

owner whoami /user | grep -q 'S-1-5-18' || { fail "the prefix owner is not SYSTEM"; exit 1; }
out=$(owner trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"

ALL="$C/ProgramData/Microsoft/Windows/Start Menu/Programs"
MINE="$C/users/$SG_OTHER/AppData/Roaming/Microsoft/Windows/Start Menu/Programs"
WIN_MINE="C:\\users\\$SG_OTHER\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs"

# ---- SYSTEM installs A for the machine; the installer's clean-up deletes its shortcut ------
out=$(owner sparse-probe.exe add 'C:\A-signed.msix')
expect "$out" status 1 "SYSTEM adds package A (for the machine)"
[ -f "$ALL/Sync A app.lnk" ] && pass "its shortcut is the all-users one" || fail "no all-users shortcut for A"
rm -f "$ALL/Sync A app.lnk"

AS='HKLM\Software\Microsoft\Active Setup\Installed Components'
UAS='HKCU\Software\Microsoft\Active Setup\Installed Components'
owner reg add "$AS\\{SG-AS-64}" /v StubPath /d 'cmd /c echo x>>C:\m\as64.txt' /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-64}" /v Version /d '1,0,0,0' /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-64}" /v IsInstalled /t REG_DWORD /d 1 /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-WHO}" /v StubPath /d 'cmd /c whoami /user >C:\m\aswho.txt' /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-32}" /v StubPath /d 'cmd /c echo x>>C:\m\as32.txt' /f /reg:32 >/dev/null
owner reg add "$AS\\{SG-AS-32}" /v Version /d '2,1' /f /reg:32 >/dev/null
owner reg add "$AS\\{SG-AS-OFF}" /v StubPath /d 'cmd /c echo x>>C:\m\asoff.txt' /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-OFF}" /v IsInstalled /t REG_DWORD /d 0 /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-HAVE}" /v StubPath /d 'cmd /c echo x>>C:\m\ashave.txt' /f /reg:64 >/dev/null
owner reg add "$AS\\{SG-AS-HAVE}" /v Version /d '3,0,0,0' /f /reg:64 >/dev/null
chmod -R g+rwX "$C/m" "$C/ProgramData" 2>/dev/null

# ---- SG_OTHER signs in -----------------------------------------------------------------------
sudo -n -u "$SG_OTHER" Xvfb ":$XN" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 &
XPID=$!
sleep 2
user reg add "$UAS\\{SG-AS-HAVE}" /v Version /d '3,0,0,0' /f >/dev/null
user reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null
user reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null
shell() {   # shell SESSION: the person's shell of that sign-in, in the background
    (cd "$C" && sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG="${SHELL_DEBUG:--all}" WINEDLLOVERRIDES="$WINEDLLOVERRIDES" \
        HOME="$T/userhome" DISPLAY=":$XN" XDG_SESSION_ID="$1" timeout -s KILL 240 "$WINE" explorer /desktop=shell,800x600 \
        >> "$T/explorer-$1.out" 2>&1 </dev/null &)
}
stop_shell() {
    sudo -n -u "$SG_OTHER" pkill -KILL -x explorer.exe 2>/dev/null
    sleep 2
}
shell 21
have_a() { [ -f "$MINE/Sync A app.lnk" ]; }
wait_for 40 have_a
sleep 3
echo "      Active Setup: as64=$(count as64) as32=$(count as32) off=$(count asoff) have=$(count ashave)"
[ "$(count as64)" = 1 ] && pass "Active Setup: the component's StubPath runs at sign-in" || fail "the StubPath ran $(count as64) times"
[ "$(count as32)" = 1 ] && pass "and the one only in the 32-bit view" || fail "the 32-bit view's ran $(count as32) times"
[ "$(count asoff)" = 0 ] && pass "not one with IsInstalled=0" || fail "IsInstalled=0 ran"
[ "$(count ashave)" = 0 ] && pass "not one the person has at its version" || fail "the person's own version ran again"
if grep -q 'S-1-5-21' "$C/m/aswho.txt" 2>/dev/null && ! grep -q 'S-1-5-18' "$C/m/aswho.txt"; then
    pass "a StubPath runs as the person, not as SYSTEM"
else fail "the StubPath's account: $(tr -d '\r' < "$C/m/aswho.txt" 2>/dev/null | tr '\n' ' ')"; fi
out=$(user reg query "$UAS\\{SG-AS-64}" /v Version)
printf '%s' "$out" | grep -q '1,0,0,0' && pass "the person's key gets the component's Version" || fail "HKCU Version: $out"

have_a && pass "A, whose all-users shortcut the installer deleted, is in $SG_OTHER's own Start menu" ||
    fail "no Start entry for A: $(ls "$MINE" "$ALL" 2>&1 | tr '\n' ' ')"
[ "$(stat -c %U "$MINE/Sync A app.lnk" 2>/dev/null)" = "$SG_OTHER" ] && pass "made by $SG_OTHER themselves (their shell's sync)" ||
    fail "the shortcut's owner: $(stat -c %U "$MINE/Sync A app.lnk" 2>/dev/null)"
# SYSTEM's own run of the sync writes into no Start menu: it is each person's
owner rundll32.exe appxdeploymentclient.dll,SgSyncStartEntries >/dev/null
made=$(find "$C/users" "$ALL" -name 'Sync A app.lnk' -not -path "*/users/$SG_OTHER/*" 2>/dev/null)
[ -z "$made" ] && pass "the sync run as SYSTEM makes no shortcut (not even its own)" || fail "SYSTEM's sync made: $made"
out=$(user sparse-probe.exe lnk "$WIN_MINE\\Sync A app.lnk")
expect "$out" target "C:\\Program Files\\WindowsApps\\$FULL_A\\msixapps-app.exe" "its shortcut runs the package's program"
out=$(user msixapps-probe.exe lnk "$WIN_MINE\\Sync A app.lnk")
expect "$out" AUMID "$FAM_A!App" "with the application's AUMID"

# ---- live: SYSTEM installs and removes B while SG_OTHER is signed in -------------------------
echo "      adding B at $(date +%T.%N)"
out=$(owner sparse-probe.exe add 'C:\B-signed.msix')
expect "$out" status 1 "SYSTEM adds package B while $SG_OTHER is signed in"
rm -f "$ALL/Sync B app.lnk"
have_b() { [ -f "$MINE/Sync B app.lnk" ]; }
lost_b() { [ ! -f "$MINE/Sync B app.lnk" ]; }
wait_for 30 have_b && pass "B is in $SG_OTHER's Start menu at once, without signing in again" || fail "no Start entry for B"
echo "      removing B at $(date +%T.%N)"
out=$(owner sparse-probe.exe remove "$FULL_B")
expect "$out" status 1 "SYSTEM removes B"
wait_for 30 lost_b && pass "and its entry leaves $SG_OTHER's Start menu" || fail "B's entry stays"

# ---- the shell again in the sign-in, the next sign-ins ----------------------------------------
stop_shell
shell 21
sleep 15
[ "$(count as64)" = 1 ] && pass "the shell started again in the sign-in runs no stub again" || fail "ran again: $(count as64)"
stop_shell
shell 22
sleep 15
[ "$(count as64)" = 1 ] && pass "the next sign-in neither (the person has it)" || fail "ran at the next sign-in: $(count as64)"
stop_shell
owner reg add "$AS\\{SG-AS-64}" /v Version /d '1,0,0,1' /f /reg:64 >/dev/null
shell 23
two() { [ "$(count as64)" = 2 ]; }
wait_for 30 two && pass "a higher Version for the machine runs it once more" || fail "after the new version: $(count as64)"
[ "$(count as32)" = 1 ] && pass "and not the others" || fail "the 32-bit one ran again: $(count as32)"

# ---- the machine's shortcut back: no second entry ------------------------------------------------
cp "$MINE/Sync A app.lnk" "$ALL/Sync A app.lnk"
lost_a() { [ ! -f "$MINE/Sync A app.lnk" ]; }
wait_for 30 lost_a && pass "A's all-users shortcut back: $SG_OTHER's copy goes (one entry)" || fail "two entries for A"

# ---- AUMID, AppsFolder ------------------------------------------------------------------------
rm -f "$C/msixapps-out.txt"
out=$(user msixapps-probe.exe activate "$FAM_A!App" "one two")
expect "$out" Activate 0x00000000 "$SG_OTHER starts A by its AUMID"
wait_for 10 test -f "$C/msixapps-out.txt"
tr -d "\r" < "$C/msixapps-out.txt" 2>/dev/null | grep -qx "family=$FAM_A" && pass "with the package's identity" ||
    fail "identity: $(tr '\n' ' ' < "$C/msixapps-out.txt" 2>/dev/null)"
rm -f "$C/msixapps-out.txt"
out=$(user msixapps-probe.exe exec "shell:AppsFolder\\$FAM_A!App")
expect "$out" Exec 1 "and from shell:AppsFolder"
wait_for 10 test -f "$C/msixapps-out.txt" && pass "which runs it" || fail "AppsFolder did not run it"
stop_shell

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
