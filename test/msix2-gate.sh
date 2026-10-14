#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# MSIX deployment, the rest of PackageManager (patch 3226): staging, registering a staged package by
# its full name, FindUsers, SetPackageState and Package.Status, VerifyContentIntegrityAsync,
# CleanupPackageForUserAsync, ProvisionPackageForAllUsersAsync, StageUserDataAsync, App Installer
# files, cancelling a deployment, the class name of the operation and AppListEntry.DisplayInfo.
# The packages are made here (test/mkmsix.py), signed with osslsigncode by a certificate that
# lasts for the run.
#
#   WINE=/opt/wine-sg/bin/wine test/msix2-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-msix2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
cleanup() {
    "$(dirname "$WINE")/wineserver" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"

B="$T/b"
"$MINGW" -O1 -o "$B/msix2-probe.exe" "$HERE/msix2-probe.c" -lruntimeobject -lshlwapi || { fail "msix2-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -shared -o "$B/sgframework.dll" "$HERE/msix-fw.c" -Wl,--out-implib,"$B/libsgframework.a" || { fail "the framework DLL did not build"; exit 1; }
"$MINGW" -O2 -o "$B/msix-app.exe" "$HERE/msix-app.c" -L"$B" -lsgframework || { fail "msix-app did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/msix2-probe.exe" "$B/trust-probe.exe" "$C/"

MK="python3 $HERE/mkmsix.py"
$MK package "$B/fw.msix" --name StainedGlass.TestFramework --version 1.0.0.0 --framework \
    --file "$B/sgframework.dll:sgframework.dll"
# a file of two whole blocks: bytes added to it leave the hash of every block as it was
head -c 131072 /dev/urandom > "$B/big.bin"
$MK package "$B/app.msix" --name StainedGlass.MsixTest --version 1.0.0.0 --display-name "MSIX test" \
    --dep StainedGlass.TestFramework:1.0.0.0 --app "App:msix-app.exe:sgmsixtest.exe:MSIX test app" \
    --file "$B/msix-app.exe:msix-app.exe" --file "$B/big.bin:big.bin"

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
for f in fw app; do
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/$f.msix" -out "$C/$f.msix" >/dev/null 2>&1 \
        || { fail "osslsigncode could not sign $f"; exit 1; }
done

run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
P=./msix2-probe.exe
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Test Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
FW="StainedGlass.TestFramework_1.0.0.0_x64__$PUBID"
APP="StainedGlass.MsixTest_1.0.0.0_x64__$PUBID"
FAMILY="StainedGlass.MsixTest_$PUBID"
WINAPPS="$C/users/$(id -un)/AppData/Local/Microsoft/WindowsApps"
loc() { find "$C" -type d -name "$1" 2>/dev/null | head -1; }
lnk() { find "$C/users" -path '*Start Menu/Programs/MSIX test app.lnk' 2>/dev/null | head -1; }

out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"

# ---- staging ----------------------------------------------------------------------------------
out=$(run $P opname 'C:\fw.msix')
case "$out" in *'IAsyncOperationWithProgress`2<Windows.Management.Deployment.DeploymentResult,Windows.Management.Deployment.DeploymentProgress>'*)
    pass "the class name of a deployment operation" ;; *) fail "operation class name: $out" ;; esac
out=$(run $P remove "$FW")
expect "$out" RemoveStatus 1 "(the framework of that test is removed again)"

out=$(run $P stage 'C:\app.msix' 'C:\fw.msix')
expect "$out" StageStatus 1 "a package and its framework are staged"
out=$(run $P find "$APP")
expect "$out" Found 0 "a staged package is not registered for the user: FindPackageByPackageFullName does not find it"
out=$(run $P listuser x)
expect "$out" Listed 0 "and FindPackages does not list it"
[ -n "$(loc "$APP")" ] && [ -f "$(loc "$APP")/msix-app.exe" ] && pass "its files are extracted" || fail "no files of the staged package"
[ ! -e "$WINAPPS/sgmsixtest.exe" ] && pass "without an alias" || fail "alias of a staged package"
[ -z "$(lnk)" ] && pass "and without a Start menu entry" || fail "Start menu entry of a staged package"
out=$(run $P users "$APP")
expect "$out" FindUsers 0x00000000 "FindUsers answers for a staged package"
expect "$out" Users 1 "with one user"
printf '%s\n' "$out" | grep -qE '^User=S-1-5-21-[0-9-]+ state=1$' && pass "whose install state is Staged" || fail "user: $out"
out=$(run $P stageuser "$APP")
expect "$out" StageUserStatus 1 "StageUserDataAsync of a staged package completes"
out=$(run $P stageuser "StainedGlass.Nothing_1.0.0.0_x64__$PUBID")
expect "$out" StageUserExtended 0x80073cf1 "of a package that is not there: ERROR_INSTALL_PACKAGE_NOT_FOUND"
out=$(run $P stage2 'C:\fw.msix')
expect "$out" StageStatus 1 "staging a staged package again completes (with options)"

# ---- registering the staged package -----------------------------------------------------------
out=$(run $P registerfull "$APP" "$FW")
expect "$out" RegisterStatus 1 "RegisterPackageByFullNameAsync registers the staged package"
out=$(run $P find "$APP")
expect "$out" Found 1 "now the user has it"
out=$(run $P find "$FW")
expect "$out" Found 1 "and its framework, registered with it"
[ -f "$WINAPPS/sgmsixtest.exe" ] && pass "its alias exists" || fail "no alias after the registration"
[ -n "$(lnk)" ] && pass "and its Start menu entry" || fail "no Start menu entry after the registration"
out=$(run $P users "$APP")
printf '%s\n' "$out" | grep -qE '^User=S-1-5-21-[0-9-]+ state=2$' && pass "the user's install state is Installed" || fail "user: $out"
out=$(run $P registerfull "$APP")
expect "$out" RegisterStatus 1 "registering a registered package completes"
out=$(run $P registerfull "StainedGlass.Nothing_1.0.0.0_x64__$PUBID")
expect "$out" RegisterExtended 0x80073cf1 "registering a package that is not there: ERROR_INSTALL_PACKAGE_NOT_FOUND"
out=$(run $P stage 'C:\app.msix' 'C:\fw.msix')
expect "$out" StageStatus 1 "staging an installed package completes"
out=$(run $P find "$APP")
expect "$out" Found 1 "and leaves it registered"
out=$(run $P users "StainedGlass.Nothing_1.0.0.0_x64__$PUBID")
expect "$out" Users 0 "FindUsers of an unknown package is an empty list"

# ---- status, state and the integrity of the content -------------------------------------------
out=$(run $P status "$APP")
expect "$out" VerifyIsOK 1 "Package.Status of an intact package: VerifyIsOK"
expect "$out" Tampered 0 "not tampered"
expect "$out" DependencyIssue 0 "its framework is there"
out=$(run $P verify "$APP")
expect "$out" Intact 1 "VerifyContentIntegrityAsync: the files are as the block map says"
exe="$(loc "$APP")/msix-app.exe"
cp "$exe" "$T/exe.bak"
printf 'x' >> "$exe"
out=$(run $P verify "$APP")
expect "$out" Intact 0 "a file with an added byte is not intact"
cp "$T/exe.bak" "$exe"
printf 'Y' | dd of="$exe" bs=1 seek=100 conv=notrunc 2>/dev/null
out=$(run $P verify "$APP")
expect "$out" Intact 0 "nor is one with a changed byte"
cp "$T/exe.bak" "$exe"
big="$(loc "$APP")/big.bin"
cp "$big" "$T/big.bak"
printf 'z' >> "$big"
out=$(run $P verify "$APP")
expect "$out" Intact 0 "a file with a byte added after its last block is not intact (the size is checked)"
cp "$T/big.bak" "$big"
out=$(run $P verify "$APP")
expect "$out" Intact 1 "(and is intact again when it is put back)"
rm -f "$(loc "$APP")/Assets/logo.png"
out=$(run $P verify "$APP")
expect "$out" Intact 0 "nor a package with a file missing"
out=$(run $P status "$APP")
expect "$out" VerifyIsOK 1 "(the state of the package is a different thing from its files)"
out=$(run $P setstate "$APP" 3)
expect "$out" SetPackageState 0x00000000 "SetPackageState(Tampered)"
out=$(run $P status "$APP")
expect "$out" Tampered 1 "the status says Tampered"
expect "$out" VerifyIsOK 0 "and is not OK"
out=$(run $P setstate "$APP" 1)
out=$(run $P status "$APP")
expect "$out" LicenseIssue 1 "LicenseInvalid is a license issue"
out=$(run $P setstate "$APP" 2)
out=$(run $P status "$APP")
expect "$out" Modified 1 "Modified"
out=$(run $P setstate "$APP" 0)
out=$(run $P status "$APP")
expect "$out" VerifyIsOK 1 "Normal: OK again"
out=$(run $P setstate "$APP" 9)
expect "$out" SetPackageState 0x80070057 "a state out of the range: E_INVALIDARG"
out=$(run $P setstate "StainedGlass.Nothing_1.0.0.0_x64__$PUBID" 0)
expect "$out" SetPackageState 0x80073cf1 "a package that is not there: ERROR_INSTALL_PACKAGE_NOT_FOUND"

# ---- the display information of an application -------------------------------------------------
# the logo is gone (removed above): put it back from the package
mkdir -p "$(loc "$APP")/Assets"
printf '\x89PNG\r\n\x1a\n' > "$(loc "$APP")/Assets/logo.png"
out=$(run $P entries "$APP")
expect "$out" Entries 1 "the package has one application in the app list"
expect "$out" get_DisplayInfo 0x00000000 "AppListEntry.DisplayInfo"
expect "$out" DisplayName "MSIX test app" "DisplayName is the one of the manifest"
expect "$out" Description test "and Description"
expect "$out" GetLogo44 "0x00000000 stream" "GetLogo(44x44) gives the logo as a stream reference"
expect "$out" GetLogo150 "0x00000000 stream" "and GetLogo(150x150)"

# ---- provisioning ----------------------------------------------------------------------------
out=$(run $P remove "$APP")
expect "$out" RemoveStatus 1 "(the application is removed)"
out=$(run $P stage 'C:\app.msix')
expect "$out" StageStatus 1 "(and staged again)"
out=$(run $P find "$APP")
expect "$out" Found 0 "(not registered)"
out=$(run $P provision "$FAMILY")
expect "$out" ProvisionStatus 1 "ProvisionPackageForAllUsersAsync of a staged package completes"
out=$(run $P find "$APP")
expect "$out" Found 1 "and registers it for the user"
out=$(run reg query "HKCU\\Software\\Wine\\AppModel\\Packages\\$APP" /v Provisioned)
case "$out" in *0x1*) pass "and records it as provisioned" ;; *) fail "provisioned: $out" ;; esac
out=$(run $P provision "StainedGlass.Nothing_$PUBID")
expect "$out" ProvisionExtended 0x80073cf1 "a family that is not staged: ERROR_INSTALL_PACKAGE_NOT_FOUND"

# ---- other users keep the files ----------------------------------------------------------------
OTHER='HKU\S-1-5-21-111-222-333-1001\Software\Wine\AppModel\Packages'
WINLOC=$(printf '%s' "$(loc "$APP")" | sed "s|^$C|C:|; s|/|\\\\|g")
run reg add "$OTHER\\$APP" /v Name /d StainedGlass.MsixTest /f >/dev/null
run reg add "$OTHER\\$APP" /v InstallLocation /d "$WINLOC" /f >/dev/null
out=$(run $P users "$APP")
expect "$out" Users 2 "a second user's profile with the package: FindUsers lists two"
out=$(run $P remove "$APP")
expect "$out" RemoveStatus 1 "the first user removes it"
[ -d "$(loc "$APP")" ] && pass "the files stay for the other user" || fail "files of a package another user has were deleted"
out=$(run $P cleanup StainedGlass.MsixTest 'S-1-5-21-111-222-333-1001')
expect "$out" CleanupStatus 1 "CleanupPackageForUserAsync for the other user completes"
[ -z "$(loc "$APP")" ] && pass "and, with no user left, the files go" || fail "files left after the last user was cleaned up"
out=$(run reg query "$OTHER\\$APP")
case "$out" in *"$APP"*) fail "the record of the other user is still there" ;; *) pass "with that user's record" ;; esac
out=$(run $P cleanup StainedGlass.MsixTest 'not a sid\x')
expect "$out" CleanupExtended 0x80070057 "a SID with a backslash: E_INVALIDARG"

# the caller's own package is cleaned up like a removal
out=$(run $P stage 'C:\app.msix')
out=$(run $P registerfull "$APP")
expect "$out" RegisterStatus 1 "(registered again)"
out=$(run $P cleanup "$APP" '')
expect "$out" CleanupStatus 1 "cleaning up a package for the calling user (no SID)"
out=$(run $P find "$APP")
expect "$out" Found 0 "removes it"

# ---- App Installer files -------------------------------------------------------------------------
cat > "$C/app.appinstaller" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<AppInstaller xmlns="http://schemas.microsoft.com/appx/appinstaller/2021" Version="1.0.0.0" Uri="file:///C:/app.appinstaller">
  <MainPackage Name="StainedGlass.MsixTest" Publisher="CN=Stained Glass Test Publisher" Version="1.0.0.0" ProcessorArchitecture="x64" Uri="file:///C:/app.msix"/>
  <Dependencies>
    <Package Name="StainedGlass.TestFramework" Publisher="CN=Stained Glass Test Publisher" Version="1.0.0.0" ProcessorArchitecture="x64" Uri="file:///C:/fw.msix"/>
  </Dependencies>
</AppInstaller>
EOF
out=$(run $P remove "$FW")
out=$(run $P appinstaller 'C:\app.appinstaller')
expect "$out" AppInstallerStatus 1 "an App Installer file installs its main package with its dependencies"
out=$(run $P find "$APP")
expect "$out" Found 1 "the main package is there"
out=$(run $P find "$FW")
expect "$out" Found 1 "and the dependency"
cat > "$C/empty.appinstaller" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<AppInstaller xmlns="http://schemas.microsoft.com/appx/appinstaller/2021" Version="1.0.0.0" Uri="file:///C:/empty.appinstaller">
</AppInstaller>
EOF
out=$(run $P appinstaller 'C:\empty.appinstaller')
expect "$out" AppInstallerExtended 0x80070057 "an App Installer file with no main package is refused (E_INVALIDARG)"
cat > "$C/bad.appinstaller" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<AppInstaller xmlns="http://schemas.microsoft.com/appx/appinstaller/2021" Version="1.0.0.0" Uri="x">
  <MainPackage Name="N" Publisher="P" Version="1.0.0.0" Uri="ftp://example.invalid/app.msix"/>
</AppInstaller>
EOF
out=$(run $P appinstaller 'C:\bad.appinstaller')
expect "$out" AppInstallerExtended 0x80070057 "a package with a Uri that is not a file, http or https location is refused"
out=$(run $P remove "$APP")
out=$(run $P remove "$FW")

# ---- cancelling ---------------------------------------------------------------------------------------
out=$(run $P addcancel 'C:\fw.msix')
expect "$out" Cancel 0x00000000 "IAsyncInfo::Cancel on a deployment in flight"
expect "$out" CancelStatus 2 "ends it as Canceled"
expect "$out" CancelAgain 0x00000000 "cancelling again does nothing"
out=$(run $P find "$FW")
expect "$out" Found 0 "nothing of the cancelled package was installed"
[ -z "$(loc "$FW")" ] && pass "and no files were left" || fail "files left after a cancelled deployment"

exit $RC
