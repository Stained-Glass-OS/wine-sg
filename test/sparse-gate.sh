#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Packages with external location ("sparse" packages) and IPackageManager7-9 (patches/sg/6026):
#
#   options     AddPackageOptions, StagePackageOptions, RegisterPackageOptions are activatable,
#               keep what is set, give vectors the caller fills
#   sparse      IPackageManager9.AddPackageByUriAsync with ExternalLocationUri: the package is
#               recorded with its external location; a program there whose manifest names the
#               package has its identity, one without that manifest or outside has none; its
#               Start menu shortcut and alias start the program in the external location
#   refusals    an external location for a package that does not allow external content
#               (ERROR_PACKAGE_EXTERNAL_LOCATION_NOT_ALLOWED), one that is no folder
#   unsigned    AllowUnsigned installs an unsigned package of the unsigned-publisher namespace,
#               refuses one outside it and a signed one inside it; without it, unsigned is refused
#   provision   FindProvisionedPackages lists a provisioned family; IPackageManager8 deprovisions it
#   volume      PackageVolume.FindPackages / ByPackageFamilyName give vectors of the packages
#   stub        the stub preference is Full, a bad value refused
#   register    RegisterPackageByUriAsync of a loose sparse manifest with an external location
#   request     IPackageManager7.RequestAddPackageAsync
#
#   WINE=/opt/wine-sg/bin/wine test/sparse-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" i686-w64-mingw32-gcc "$WINDRES" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-sparse.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
    LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"

PUB='CN=Stained Glass Test Publisher'
UPUB='CN=Stained Glass Unsigned, OID.2.25.311729368913984317654407730594956997722=1'
pubid() {
    python3 -c "
import hashlib,sys
h=hashlib.sha256(sys.argv[1].encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))" "$1"
}
PUBID=$(pubid "$PUB")
UPUBID=$(pubid "$UPUB")
SPARSE="StainedGlass.SparseTest_1.0.0.0_x64__$PUBID"
LOOSE="StainedGlass.SparseLoose_1.0.0.0_x64__$PUBID"
PROV="StainedGlass.ProvTest_1.0.0.0_x64__$PUBID"
UOK="StainedGlass.UnsignedTest_1.0.0.0_x64__$UPUBID"
REQ="StainedGlass.RequestTest_1.0.0.0_x64__$PUBID"

"$MINGW" -O2 -o "$B/sparse-probe.exe" "$HERE/sparse-probe.c" -lruntimeobject -lshlwapi -lole32 -luuid ||
    { fail "sparse-probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$B/sparse-probe32.exe" "$HERE/sparse-probe.c" -lruntimeobject -lshlwapi -lole32 -luuid ||
    { fail "the 32-bit sparse-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
# the program in the external location: the probe, with a manifest naming the package
cat > "$B/sparse-app.manifest" <<EOF
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0" name="StainedGlass.SparseApp" type="win32"/>
  <msix xmlns="urn:schemas-microsoft-com:msix.v1" publisher="$PUB" packageName="StainedGlass.SparseTest" applicationId="App"/>
</assembly>
EOF
sed 's/StainedGlass.SparseTest/StainedGlass.SparseLoose/' "$B/sparse-app.manifest" > "$B/loose-app.manifest"
printf '1 24 "%s"\n' "$B/sparse-app.manifest" > "$B/sparse-app.rc"
printf '1 24 "%s"\n' "$B/loose-app.manifest" > "$B/loose-app.rc"
"$WINDRES" -O coff -i "$B/sparse-app.rc" -o "$B/sparse-app.res" && "$WINDRES" -O coff -i "$B/loose-app.rc" -o "$B/loose-app.res" ||
    { fail "the manifest resources did not build"; exit 1; }
"$MINGW" -O2 -o "$B/sparse-app.exe" "$HERE/sparse-probe.c" "$B/sparse-app.res" -lruntimeobject -lshlwapi -lole32 -luuid &&
"$MINGW" -O2 -o "$B/loose-app.exe" "$HERE/sparse-probe.c" "$B/loose-app.res" -lruntimeobject -lshlwapi -lole32 -luuid ||
    { fail "sparse-app did not build"; exit 1; }

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$B/sparse-probe.exe" "$B/sparse-probe32.exe" "$B/trust-probe.exe" "$B/sparse-app.exe" "$C/"
mkdir -p "$C/SparseApp" "$C/Loose"
cp "$B/sparse-app.exe" "$B/sparse-probe.exe" "$C/SparseApp/"
cp "$B/loose-app.exe" "$C/SparseApp/"

# ---- fixtures -----------------------------------------------------------------
MK="python3 $HERE/mkmsix.py"
$MK package "$B/sparse.msix" --name StainedGlass.SparseTest --version 1.0.0.0 --allow-external \
    --display-name "Sparse test" --app "App:sparse-app.exe:sgsparse.exe:Sparse test app"
$MK package "$B/loose.msix" --name StainedGlass.SparseLoose --version 1.0.0.0 --allow-external \
    --display-name "Sparse loose" --app "App:loose-app.exe::Sparse loose app"
$MK package "$B/plain.msix" --name StainedGlass.PlainTest --version 1.0.0.0 --app "App:sparse-app.exe::Plain test app" \
    --file "$B/sparse-app.exe:sparse-app.exe"
$MK package "$B/prov.msix" --name StainedGlass.ProvTest --version 1.0.0.0 --app "App:sparse-app.exe::Prov test app" \
    --file "$B/sparse-app.exe:sparse-app.exe"
$MK package "$B/req.msix" --name StainedGlass.RequestTest --version 1.0.0.0 --app "App:sparse-app.exe::Request test app" \
    --file "$B/sparse-app.exe:sparse-app.exe"
$MK package "$C/unsigned-oid.msix" --name StainedGlass.UnsignedTest --version 1.0.0.0 --allow-external \
    --publisher "$UPUB" --app "App:sparse-app.exe::Unsigned test app"
$MK package "$C/unsigned-plain.msix" --name StainedGlass.UnsignedPlain --version 1.0.0.0 --allow-external \
    --app "App:sparse-app.exe::Unsigned plain app"
$MK package "$B/signed-oid.msix" --name StainedGlass.SignedOid --version 1.0.0.0 --allow-external \
    --publisher "$UPUB" --app "App:sparse-app.exe::Signed oid app"
(cd "$T" && python3 -c "import zipfile,sys; open(sys.argv[2],'wb').write(zipfile.ZipFile(sys.argv[1]).read('AppxManifest.xml'))" \
    "$B/loose.msix" "$C/Loose/AppxManifest.xml")

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
for f in sparse.msix plain.msix prov.msix req.msix signed-oid.msix; do
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/$f" -out "$C/$f" >/dev/null 2>&1 \
        || { fail "osslsigncode could not sign $f"; exit 1; }
done

run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"

# ---- the option classes ---------------------------------------------------------
out=$(run sparse-probe.exe options)
expect "$out" add yes "AddPackageOptions is activatable"
expect "$out" stage yes "StagePackageOptions is activatable"
expect "$out" register yes "RegisterPackageOptions is activatable"
expect "$out" add.class Windows.Management.Deployment.AddPackageOptions "and names its class"
expect "$out" add.options2 yes "AddPackageOptions has IAddPackageOptions2"
expect "$out" add.stage no "but not another class's interface"
expect "$out" add.external null "no external location by default"
expect "$out" add.allow_unsigned false "unsigned packages not allowed by default"
expect "$out" add.stub 0 "the stub option is Default"
expect "$out" add.stub_bad 80070057 "a stub option out of range is refused"
expect "$out" add.deps.vector yes "the dependency URIs are an IVector<Uri>"
expect "$out" add.deps.size 0 "empty at first"
expect "$out" add.deps.kept 1 "and the URI appended stays in it"
expect "$out" add.names.vector yes "the optional family names are an IVector<String>"
expect "$out" add.external.kept yes "the external location set is kept"
expect "$out" add.allow_unsigned.kept true "AllowUnsigned set is kept"
expect "$out" register.external.kept yes "RegisterPackageOptions keeps its external location"

# ---- a package with external location -----------------------------------------
out=$(run sparse-probe.exe add 'C:\sparse.msix' 'C:\SparseApp\')
expect "$out" pm9 yes "PackageManager has IPackageManager9"
expect "$out" status 1 "AddPackageByUriAsync adds the sparse package with its external location"
out=$(run reg query "HKCU\\Software\\Wine\\AppModel\\Packages\\$SPARSE" /v ExternalLocation)
printf '%s\n' "$out" | grep -q 'ExternalLocation.*REG_SZ.*C:\\SparseApp$' && pass "its external location is recorded (as a folder, no backslash after it)" ||
    fail "no external location recorded: $(printf '%s' "$out" | tr '\n' ' ')"
out=$(run sparse-probe.exe info "$SPARSE")
printf '%s\n' "$out" | grep -q "^location=.*WindowsApps\\\\$SPARSE\$" && pass "its installed location is the package's folder" ||
    fail "installed location: $(printf '%s' "$out" | tr '\n' ' ')"
out=$(run 'C:\SparseApp\sparse-app.exe' identity)
expect "$out" identity "$SPARSE" "the program in the external location whose manifest names the package has its identity"
out=$(run 'C:\SparseApp\sparse-probe.exe' identity)
expect "$out" identity none "a program there without that manifest has none"
out=$(run 'C:\sparse-app.exe' identity)
expect "$out" identity none "nor has the same program outside the external location"
lnk=$(find "$C/users" -path '*Start Menu/Programs/Sparse test app.lnk' 2>/dev/null | head -1)
if [ -n "$lnk" ]; then
    out=$(run sparse-probe.exe lnk "$(printf '%s' "$lnk" | sed "s|^$C|C:|; s|/|\\\\|g")")
    expect "$out" target 'C:\SparseApp\sparse-app.exe' "the Start menu shortcut starts the program in the external location"
else fail "no Start menu shortcut for the sparse package"; fi
out=$(run cmd /c sgsparse.exe identity)
expect "$out" identity "$SPARSE" "its alias starts the program in the external location, with the identity"

out=$(run sparse-probe.exe volume "StainedGlass.SparseTest_$PUBID")
expect "$out" all.vector yes "PackageVolume.FindPackages gives an IVector<Package>"
expect "$out" family 1 "FindPackagesByPackageFamilyName finds the package"
expect "$out" family.0 "$SPARSE" "by its full name"
out=$(run sparse-probe.exe status "StainedGlass.SparseTest_$PUBID")
expect "$out" package.0 "$SPARSE" "FindPackagesByUserSecurityIdPackageFamilyName finds it, walked as Dropbox does"
expect "$out" ok.0 1 "its status is OK"
expect "$out" status2.0 00000000 "its status has IPackageStatus2 (Dropbox used the NULL it got and crashed)"
expect "$out" partial.0 0 "and it is not partially staged"
out=$(run sparse-probe.exe stub "StainedGlass.SparseTest_$PUBID")
expect "$out" preference 0 "the stub preference is Full"
expect "$out" set_bad 80070057 "a bad preference is refused"
expect "$out" set_full 00000000 "Full may be set"

# ---- refusals -------------------------------------------------------------------
out=$(run sparse-probe.exe add 'C:\plain.msix' 'C:\SparseApp')
expect "$out" extended 80073d2e "an external location for a package without external content is refused"
out=$(run sparse-probe.exe add 'C:\sparse.msix' 'C:\NoSuchFolder')
expect "$out" extended 80070003 "an external location that is not a folder is refused"

# ---- unsigned packages ------------------------------------------------------------
out=$(run sparse-probe.exe add 'C:\unsigned-oid.msix' 'C:\SparseApp')
expect "$out" extended 800b0100 "an unsigned package is refused without AllowUnsigned"
out=$(run sparse-probe.exe add 'C:\unsigned-oid.msix' 'C:\SparseApp' unsigned)
expect "$out" status 1 "with AllowUnsigned an unsigned package of the unsigned namespace installs"
out=$(run sparse-probe.exe info "$UOK")
expect "$out" found yes "and is found"
out=$(run sparse-probe.exe add 'C:\unsigned-plain.msix' 'C:\SparseApp' unsigned)
expect "$out" extended 80073d2c "an unsigned package outside that namespace is refused"
out=$(run sparse-probe.exe add 'C:\signed-oid.msix' 'C:\SparseApp' unsigned)
expect "$out" extended 80073d2d "a signed package inside it is refused"

# ---- provisioning ---------------------------------------------------------------------
out=$(run sparse-probe.exe stage 'C:\prov.msix')
expect "$out" status 1 "StagePackageByUriAsync stages a package"
out=$(run sparse-probe.exe provision "StainedGlass.ProvTest_$PUBID")
expect "$out" status 1 "it is provisioned for all users"
out=$(run sparse-probe.exe provisioned)
printf '%s\n' "$out" | grep -qx "provisioned\\.[0-9]*=$PROV" && pass "FindProvisionedPackages lists it" ||
    fail "provisioned packages: $(printf '%s' "$out" | tr '\n' ' ')"
expect "$out" provisioned.vector yes "as an IVector<Package>"
out=$(run sparse-probe.exe deprovision "StainedGlass.ProvTest_$PUBID")
expect "$out" status 1 "IPackageManager8 deprovisions it"
out=$(run sparse-probe.exe provisioned)
printf '%s\n' "$out" | grep -q "=$PROV\$" && fail "still provisioned: $(printf '%s' "$out" | tr '\n' ' ')" ||
    pass "and FindProvisionedPackages no longer lists it"
out=$(run sparse-probe.exe info "$PROV")
expect "$out" found yes "the user who has it keeps it"
out=$(run sparse-probe.exe deprovision "StainedGlass.ProvTest_$PUBID")
expect "$out" extended 80073cf1 "deprovisioning a family that is not provisioned fails"

# ---- IPackageManager7, removal, a loose sparse manifest -----------------------------------
out=$(run sparse-probe32.exe request 'C:\req.msix')
expect "$out" status 1 "IPackageManager7.RequestAddPackageAsync adds a package (a 32-bit caller)"
out=$(run sparse-probe.exe info "$REQ")
printf '%s\n' "$out" | grep -q '^location=C:\\Program Files\\WindowsApps\\' && pass "into the machine's Program Files, not Program Files (x86)" ||
    fail "a 32-bit caller's package location: $(printf '%s' "$out" | tr '\n' ' ')"
out=$(run sparse-probe.exe remove "$SPARSE")
expect "$out" status 1 "the sparse package is removed"
out=$(run 'C:\SparseApp\sparse-app.exe' identity)
expect "$out" identity none "and the program in its external location has no identity any more"
run reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\AppModelUnlock' /v AllowDevelopmentWithoutDevLicense /t REG_DWORD /d 1 /f >/dev/null
out=$(run sparse-probe.exe register 'C:\Loose\AppxManifest.xml' 'C:\SparseApp')
expect "$out" status 1 "RegisterPackageByUriAsync registers a loose sparse manifest with an external location"
out=$(run 'C:\SparseApp\loose-app.exe' identity)
expect "$out" identity "$LOOSE" "whose program in the external location has its identity"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
