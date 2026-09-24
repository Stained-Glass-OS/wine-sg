#!/bin/sh
# AppX/MSIX packages, and the signature trust underneath them
# (patches/sg/0034-0041).
#
#   reader   appxpackaging.dll: identity, family name, properties, payload
#            extraction; a block map that disagrees with the archive, a byte
#            changed under an intact CRC, a missing manifest are all refused
#   trust    WinVerifyTrust on an MSIX (the AppX SIP): a package signed by an
#            unknown issuer is refused; once an administrator adds the root
#            it is trusted -- in every later process too; a tampered package
#            fails its digest
#
# The fixtures are made here: test/mkappx.py writes the packages and they are
# signed with osslsigncode by a certificate generated for this run and thrown
# away with it (no key is ever committed). If NETWORK=1, winget's real
# community source (Microsoft-signed) is fetched and must verify as well.
#
#   WINE=/opt/wine-sg/bin/wine test/appx-gate.sh
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

T=$(mktemp -d /var/tmp/sg-appx.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"

"$MINGW" -O2 -o "$T/appx-probe.exe" "$HERE/appx-probe.c" -lole32 -lshlwapi || { fail "appx-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/pkgname-probe.exe" "$HERE/pkgname-probe.c" || { fail "pkgname-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/appmodel-probe.exe" "$HERE/appmodel-probe.c" -lruntimeobject || { fail "appmodel-probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T"/*.exe "$C/"

# fixtures
for v in good extra nomanifest blocktamper; do python3 "$HERE/mkappx.py" "$C/$v.msix" "$v"; done
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$C/good.msix" -out "$C/signed.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the fixture"; exit 1; }
python3 - "$C/signed.msix" "$C/signed-tampered.msix" <<'EOF'
import sys, zipfile
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
i = zipfile.ZipFile(src).getinfo('Public/data.bin')
d[i.header_offset + 30 + len(i.filename) + len(i.extra) + 500] ^= 1
open(dst, 'wb').write(d)
EOF

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Test Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")

# ---- the reader ----------------------------------------------------------
out=$(run appx-probe.exe info 'C:\good.msix')
expect "$out" CreatePackageReader 0 "a package opens"
expect "$out" Name StainedGlass.AppxTest "identity: name"
expect "$out" Version 1.2.3.4 "identity: version"
expect "$out" Architecture 9 "identity: architecture x64"
expect "$out" FamilyName "StainedGlass.AppxTest_$PUBID" "family name (publisher ID from the SHA-256 of the publisher)"
expect "$out" FullName "StainedGlass.AppxTest_1.2.3.4_x64__$PUBID" "full name"
expect "$out" DisplayName "Stained Glass AppX test" "properties: display name"
expect "$out" OSMinVersion 10.0.17763.0 "prerequisite: OS min version"
expect "$out" AUMID "StainedGlass.AppxTest_$PUBID!App" "application user model ID"
expect "$out" AppDisplayName "AppX test app" "application: VisualElements"
expect "$out" PayloadCount 2 "payload files exclude the footprint"
expect "$out" FirstPayloadType application/octet-stream "content type from [Content_Types].xml"
expect "$out" Signature 0x80080203 "an unsigned package has no signature footprint file"

out=$(run appx-probe.exe extract 'C:\good.msix' 'Public\data.bin' 'C:\data.out')
expect "$out" Size 204800 "payload size"
if python3 - "$C/good.msix" "$C/data.out" <<'EOF'
import sys, zipfile
sys.exit(0 if zipfile.ZipFile(sys.argv[1]).read('Public/data.bin') == open(sys.argv[2], 'rb').read() else 1)
EOF
then pass "a multi-block deflated payload extracts byte for byte"; else fail "extracted payload differs"; fi

out=$(run appx-probe.exe extract 'C:\blocktamper.msix' 'Public\data.bin' 'C:\bad.out')
expect "$out" GetStream 0x80080207 "a byte changed under an intact CRC fails the block map"
out=$(run appx-probe.exe info 'C:\extra.msix')
expect "$out" CreatePackageReader 0x80080205 "a file the block map does not list is refused"
out=$(run appx-probe.exe info 'C:\nomanifest.msix')
expect "$out" CreatePackageReader 0x80080203 "no manifest: APPX_E_MISSING_REQUIRED_FILE"
out=$(run appx-probe.exe bundle 'C:\good.msix')
expect "$out" CreateBundleReader 0x80080203 "a package is not a bundle"

out=$(run pkgname-probe.exe)
expect "$out" FamilyFromFull "Microsoft.Winget.Source_8wekyb3d8bbwe" "PackageFamilyNameFromFullName"
expect "$out" FullFromId "Microsoft.Winget.Source_2026.924.610.8_neutral__8wekyb3d8bbwe" \
    "PackageFullNameFromId computes the publisher ID (known answer 8wekyb3d8bbwe)"
expect "$out" NameAndPublisherId "Microsoft.Winget.Source|8wekyb3d8bbwe" "PackageNameAndPublisherIdFromFamilyName"
expect "$out" VerifyBad 87 "VerifyPackageFullName refuses a malformed name"

# ---- the package catalog, deployment queries, the .msi association -------
out=$(run appmodel-probe.exe)
expect "$out" OpenForCurrentUser 0 "PackageCatalog.OpenForCurrentUser"
expect "$out" OpenForCurrentPackage 0x80073d54 "OpenForCurrentPackage outside a package: APPMODEL_ERROR_NO_PACKAGE"
expect "$out" addStatusChanged "0 token=nonzero" "a catalog event can be subscribed"
expect "$out" removeStatusChanged 0 "and unsubscribed"
expect "$out" IWeakReferenceSource 0 "the catalog gives weak references (C++/WinRT auto-revoke)"
expect "$out" ResolveAlive object "a weak reference resolves while the catalog lives"
expect "$out" ResolveDead null "and to null once it is gone"
expect "$out" FindPackages 0 "PackageManager.FindPackages answers"
expect "$out" FindPackagesEmpty 1 "with no packages, since none has been deployed"
out=$(cd "$C" && timeout -s KILL 60 "$WINE" reg query 'HKCR\Msi.Package\shell\Open\command' 2>/dev/null | tr -d '\r')
case "$out" in *'"%1" %*'*) pass "opening a .msi passes the caller's parameters to msiexec (%*)" ;;
    *) fail "the .msi open command drops parameters: $out" ;; esac

# ---- trust -----------------------------------------------------------------
out=$(run trust-probe.exe 'C:\good.msix')
expect "$out" WinVerifyTrust 0x800b0100 "an unsigned package: TRUST_E_NOSIGNATURE"
out=$(run trust-probe.exe 'C:\signed.msix')
expect "$out" WinVerifyTrust 0x800b010a "signed by an unknown issuer: refused (CERT_E_CHAINING)"
out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "an administrator adds the issuing root to the machine"
out=$(run trust-probe.exe 'C:\signed.msix')
expect "$out" WinVerifyTrust 0 "now trusted"
out=$(run trust-probe.exe 'C:\signed.msix')
expect "$out" WinVerifyTrust 0 "still trusted in a later process (the root was kept)"
out=$(run trust-probe.exe 'C:\signed-tampered.msix')
expect "$out" WinVerifyTrust 0x80096010 "one bit changed in the payload: TRUST_E_BAD_DIGEST"

if [ "${NETWORK:-0}" = 1 ]; then
    if curl -fsSL -o "$C/source2.msix" https://cdn.winget.microsoft.com/cache/source2.msix; then
        out=$(run trust-probe.exe 'C:\source2.msix')
        expect "$out" WinVerifyTrust 0 "winget's community source (Microsoft-signed) verifies"
        out=$(run appx-probe.exe info 'C:\source2.msix')
        expect "$out" FamilyName Microsoft.Winget.Source_8wekyb3d8bbwe "winget's source: family name"
    else
        echo "info  could not fetch winget's source; skipping"
    fi
fi

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
