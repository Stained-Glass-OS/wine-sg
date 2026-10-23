#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Packages an installer adds as SYSTEM are the machine's (patches/sg/6036). On Stained Glass OS a
# per-machine installer runs elevated as the machine account (S-1-5-18), not as the user who asked
# for it: TeraCopy's MSI custom action adds its sparse package from there.
#
# On a shared system prefix (the multi-user machine; the prefix owner's token is SYSTEM):
#   record    a 32-bit caller (msiexec's custom action) as SYSTEM records the package under
#             HKLM\Software\Wine\AppModel\Packages (the 64-bit view), not in its own HKCU
#   start     its Start menu entry is the all-users one (ProgramData)
#   classes   its COM classes are the machine's (HKLM\Software\Classes, 64-bit view)
#   user      the second Unix user SG_OTHER (default sgconf) finds the package (FindPackages), runs the
#             program in its external location with the package identity, gets its context menu
#             command and creates its class; they cannot remove it
#   removal   SYSTEM removes it: record, shortcut and classes go
# Without that user (or sudo to it) the gate skips.
#   WINE=... test/sysappx-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" i686-w64-mingw32-gcc "$WINDRES" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null ||
    { echo "SKIP: no second user $SG_OTHER (sudo -n -u $SG_OTHER)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-sysappx.XXXXXX)
chmod 755 "$T"
PFX=$T/prefix
B=$T/b
export WINEPREFIX=$PFX WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
    LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY WAYLAND_DISPLAY
# shellcheck disable=SC2317  # invoked via trap
cleanup() { set +e; "$WINESERVER" -k 2>/dev/null; sleep 1; [ -n "${KEEP:-}" ] || sudo -n rm -rf "$T"; }
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
FAMILY="StainedGlass.SysTest_$(pubid "$PUB")"
FULL="StainedGlass.SysTest_1.0.0.0_x64__$(pubid "$PUB")"
C1='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0001}'

cat > "$B/sys-app.manifest" <<EOF
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0" name="StainedGlass.SysApp" type="win32"/>
  <msix xmlns="urn:schemas-microsoft-com:msix.v1" publisher="$PUB" packageName="StainedGlass.SysTest" applicationId="App"/>
</assembly>
EOF
printf '1 24 "%s"\n' "$B/sys-app.manifest" > "$B/sys-app.rc"
"$WINDRES" -O coff -i "$B/sys-app.rc" -o "$B/sys-app.res" &&
"$MINGW" -O2 -o "$B/sys-app.exe" "$HERE/sparse-probe.c" "$B/sys-app.res" -lruntimeobject -lshlwapi -lole32 -luuid &&
"$MINGW" -O2 -o "$B/sparse-probe.exe" "$HERE/sparse-probe.c" -lruntimeobject -lshlwapi -lole32 -luuid &&
i686-w64-mingw32-gcc -O2 -o "$B/sparse-probe32.exe" "$HERE/sparse-probe.c" -lruntimeobject -lshlwapi -lole32 -luuid &&
"$MINGW" -O2 -o "$B/pkgverb-probe.exe" "$HERE/pkgverb-probe.c" -lole32 -lshell32 -luuid &&
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 &&
"$MINGW" -O2 -shared -o "$B/pkgverb.dll" "$HERE/pkgverb-cmd.c" -lole32 -luuid -lshell32 || { fail "the probes did not build"; exit 1; }

cat > "$B/ext.xml" <<'EOF'
<com:Extension Category="windows.comServer"><com:ComServer>
<com:SurrogateServer DisplayName="SG verbs">
<com:Class Id="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0001" Path="pkgverb.dll" ThreadingModel="STA"/>
</com:SurrogateServer></com:ComServer></com:Extension>
<desktop4:Extension Category="windows.fileExplorerContextMenus"><desktop4:FileExplorerContextMenus>
<desktop5:ItemType Type="*"><desktop5:Verb Id="SgVerb" Clsid="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0001"/></desktop5:ItemType>
</desktop4:FileExplorerContextMenus></desktop4:Extension>
EOF
python3 "$HERE/mkmsix.py" package "$B/sys.msix" --name StainedGlass.SysTest --version 1.0.0.0 --allow-external \
    --display-name "Sys test" --app "App:sys-app.exe::Sys test app" --extensions "$B/ext.xml"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" -subj "/$(printf '%s' "$PUB")" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/sys.msix" -out "$B/sys-signed.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign"; exit 1; }

# ---- the shared system prefix -----------------------------------------------------------
mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
C=$PFX/drive_c
cp "$B/sparse-probe.exe" "$B/sparse-probe32.exe" "$B/pkgverb-probe.exe" "$B/trust-probe.exe" "$B/sys-signed.msix" "$C/"
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
mkdir -p "$C/SysApp" "$C/Data"
cp "$B/sys-app.exe" "$B/pkgverb.dll" "$C/SysApp/"
echo text > "$C/Data/a.txt"
chmod -R g+rwX "$PFX" 2>/dev/null
# the user's profile folders (a real machine makes them at their first sign-in)
sudo -n -u "$SG_OTHER" mkdir -p "$C/users/$SG_OTHER/Desktop" "$C/users/$SG_OTHER/Documents" \
    "$C/users/$SG_OTHER/AppData/Roaming" "$C/users/$SG_OTHER/AppData/Local/Temp"

owner() { (cd "$C" && sg "$SG_GROUP" -c "umask 002; timeout -s KILL 180 '$WINE' $(printf "'%s' " "$@")" 2>/dev/null </dev/null | tr -d '\r'); }
user() { (cd "$C" && sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="$WINEDLLOVERRIDES" \
              HOME="$T/userhome" timeout -s KILL 180 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
lacks() {    # lacks OUTPUT LINE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2"; then fail "$3: '$2' is there"; else pass "$3"; fi
}
haskey() { owner reg query "$1" | grep -q '^HKEY'; }
regval() { owner reg query "$1" ${2:+/v "$2"} | grep -i "REG_SZ" | sed 's/.*REG_SZ *//' | head -1; }

owner whoami /user | grep -q 'S-1-5-18' && pass "the prefix owner is the machine account (S-1-5-18)" ||
    { fail "the prefix owner is not SYSTEM: $(owner whoami /user | tr '\n' ' ')"; exit 1; }
user whoami /user | grep -q 'S-1-5-18' && { fail "$SG_OTHER is SYSTEM too"; exit 1; }
out=$(owner trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"

# ---- SYSTEM adds the package (a 32-bit caller, as msiexec's custom action) ---------------
out=$(owner sparse-probe32.exe add 'C:\sys-signed.msix' 'C:\SysApp\')
expect "$out" status 1 "SYSTEM adds the package with its external location (32-bit caller)"
haskey "HKLM\\Software\\Wine\\AppModel\\Packages\\$FULL" && pass "it is recorded for the machine (HKLM)" ||
    fail "no machine record"
haskey "HKLM\\Software\\WOW6432Node\\Wine\\AppModel\\Packages\\$FULL" && fail "the record went to the 32-bit view" ||
    pass "in the 64-bit view, not WOW6432Node"
haskey "HKCU\\Software\\Wine\\AppModel\\Packages\\$FULL" && fail "SYSTEM's own HKCU has a record too" ||
    pass "not as SYSTEM's own package"
[ -f "$C/ProgramData/Microsoft/Windows/Start Menu/Programs/Sys test app.lnk" ] &&
    pass "its Start menu entry is the all-users one" ||
    fail "no all-users Start menu entry: $(find "$C" -name 'Sys test app.lnk' 2>/dev/null | sed "s|$C||")"
[ "$(regval "HKLM\\Software\\Classes\\CLSID\\$C1\\InprocServer32")" = 'C:\SysApp\pkgverb.dll' ] &&
    pass "its class is the machine's, in the external location" ||
    fail "class: $(regval "HKLM\\Software\\Classes\\CLSID\\$C1\\InprocServer32")"

# ---- the standard user ---------------------------------------------------------------------
out=$(user sparse-probe.exe volume "$FAMILY")
expect "$out" family.0 "$FULL" "$SG_OTHER finds the machine's package by its family"
expect "$out" family 1 "once"
out=$(user sparse-probe.exe status "$FAMILY")
expect "$out" package.0 "$FULL" "$SG_OTHER walks the machine's package as Dropbox does"
expect "$out" status2.0 00000000 "its status has IPackageStatus2"
# a stale registration of the family for the user (an update's, whose files are gone): one version shows
STALE="StainedGlass.SysTest_2.0.0.0_x64__${FAMILY##*_}"
SK="HKCU\\Software\\Wine\\AppModel\\Packages\\$STALE"
user reg add "$SK" /v Name /d StainedGlass.SysTest /f >/dev/null
user reg add "$SK" /v Publisher /d "$PUB" /f >/dev/null
user reg add "$SK" /v FamilyName /d "$FAMILY" /f >/dev/null
user reg add "$SK" /v InstallLocation /d 'C:\NoSuchFolder\gone' /f >/dev/null
user reg add "$SK" /v Version /t REG_QWORD /d 0x0002000000000000 /f >/dev/null
out=$(user sparse-probe.exe status "$FAMILY")
expect "$out" count 1 "with a stale newer registration of the user's beside it, the family shows once"
expect "$out" package.0 "$FULL" "as the version whose content is there"
user reg delete "$SK" /f >/dev/null
out=$(user 'C:\SysApp\sys-app.exe' identity)
expect "$out" identity "$FULL" "the program in its external location has the package identity for $SG_OTHER"
out=$(user pkgverb-probe.exe create "$C1")
expect "$out" create 00000000 "$SG_OTHER creates the package's class"
out=$(user pkgverb-probe.exe menu 'C:\Data\a.txt')
expect "$out" item "SG Package Verb|enabled" "$SG_OTHER's context menu has the package's command"
out=$(user sparse-probe.exe remove "$FULL")
lacks "$out" "status=1" "$SG_OTHER cannot remove the machine's package"
haskey "HKLM\\Software\\Wine\\AppModel\\Packages\\$FULL" && [ -f "$C/SysApp/sys-app.exe" ] &&
    [ -f "$C/Program Files/WindowsApps/$FULL/AppxManifest.xml" ] &&
    [ -f "$C/ProgramData/Microsoft/Windows/Start Menu/Programs/Sys test app.lnk" ] &&
    pass "it is still installed, with its files and Start menu entry" || fail "the user's remove took (part of) it"

# ---- SYSTEM removes it --------------------------------------------------------------------
out=$(owner sparse-probe32.exe remove "$FULL")
expect "$out" status 1 "SYSTEM removes the package"
haskey "HKLM\\Software\\Wine\\AppModel\\Packages\\$FULL" && fail "the record is still there" || pass "its record goes"
[ -f "$C/ProgramData/Microsoft/Windows/Start Menu/Programs/Sys test app.lnk" ] && fail "the Start menu entry stays" ||
    pass "its Start menu entry goes"
haskey "HKLM\\Software\\Classes\\CLSID\\$C1" && fail "the class stays" || pass "its class goes"
out=$(user pkgverb-probe.exe menu 'C:\Data\a.txt')
lacks "$out" "item=SG Package Verb|enabled" "$SG_OTHER's menu no longer has the command"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
